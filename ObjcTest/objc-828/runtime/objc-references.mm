/*
 * Copyright (c) 2004-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
  Implementation of the weak / associative references for non-GC mode.
*/


#include "objc-private.h"
#include <objc/message.h>
#include <map>
#include "DenseMapExtras.h"

// expanded policy bits.

enum {
    OBJC_ASSOCIATION_SETTER_ASSIGN      = 0,
    OBJC_ASSOCIATION_SETTER_RETAIN      = 1,
    OBJC_ASSOCIATION_SETTER_COPY        = 3,            // NOTE:  both bits are set, so we can simply test 1 bit in releaseValue below.
    OBJC_ASSOCIATION_GETTER_READ        = (0 << 8),
    OBJC_ASSOCIATION_GETTER_RETAIN      = (1 << 8),
    OBJC_ASSOCIATION_GETTER_AUTORELEASE = (2 << 8),
    OBJC_ASSOCIATION_SYSTEM_OBJECT      = _OBJC_ASSOCIATION_SYSTEM_OBJECT, // 1 << 16
};

spinlock_t AssociationsManagerLock;

namespace objc {

class ObjcAssociation {
    uintptr_t _policy;
    id _value;
public:
    ObjcAssociation(uintptr_t policy, id value) : _policy(policy), _value(value) {}
    ObjcAssociation() : _policy(0), _value(nil) {}
    ObjcAssociation(const ObjcAssociation &other) = default;
    ObjcAssociation &operator=(const ObjcAssociation &other) = default;
    ObjcAssociation(ObjcAssociation &&other) : ObjcAssociation() {
        swap(other);
    }

    inline void swap(ObjcAssociation &other) {
        std::swap(_policy, other._policy);
        std::swap(_value, other._value);
    }

    inline uintptr_t policy() const { return _policy; }
    inline id value() const { return _value; }

    inline void acquireValue() {
        if (_value) {
            switch (_policy & 0xFF) {
            case OBJC_ASSOCIATION_SETTER_RETAIN:
                _value = objc_retain(_value);
                break;
            case OBJC_ASSOCIATION_SETTER_COPY:
                _value = ((id(*)(id, SEL))objc_msgSend)(_value, @selector(copy));
                break;
            }
        }
    }

    inline void releaseHeldValue() {
        if (_value && (_policy & OBJC_ASSOCIATION_SETTER_RETAIN)) {
            objc_release(_value);
        }
    }

    inline void retainReturnedValue() {
        if (_value && (_policy & OBJC_ASSOCIATION_GETTER_RETAIN)) {
            objc_retain(_value);
        }
    }

    inline id autoreleaseReturnedValue() {
        if (slowpath(_value && (_policy & OBJC_ASSOCIATION_GETTER_AUTORELEASE))) {
            return objc_autorelease(_value);
        }
        return _value;
    }
};

typedef DenseMap<const void *, ObjcAssociation> ObjectAssociationMap;
typedef DenseMap<DisguisedPtr<objc_object>, ObjectAssociationMap> AssociationsHashMap;

// class AssociationsManager manages a lock / hash table singleton pair.
// Allocating an instance acquires the lock

class AssociationsManager {
    using Storage = ExplicitInitDenseMap<DisguisedPtr<objc_object>, ObjectAssociationMap>;
    static Storage _mapStorage;

public:
    AssociationsManager()   { AssociationsManagerLock.lock(); }
    ~AssociationsManager()  { AssociationsManagerLock.unlock(); }

    AssociationsHashMap &get() {
        return _mapStorage.get();
    }

    static void init() {
        _mapStorage.init();
    }
};

AssociationsManager::Storage AssociationsManager::_mapStorage;

} // namespace objc

using namespace objc;

void
_objc_associations_init()
{
    AssociationsManager::init();
}

id
_object_get_associative_reference(id object, const void *key)
{
    ObjcAssociation association{};

    {
        //管理对象的管理类
        AssociationsManager manager;
        //存储所有管理对象的总表
        AssociationsHashMap &associations(manager.get());
        //以当前对象地址为key 查找 返回一个关联对象表的迭代器
        AssociationsHashMap::iterator i = associations.find((objc_object *)object);
        if (i != associations.end()) {
            //关联对象表
            ObjectAssociationMap &refs = i->second;
            //查找
            ObjectAssociationMap::iterator j = refs.find(key);
            if (j != refs.end()) {
                //取出association
                association = j->second;
                association.retainReturnedValue();
            }
        }
    }
    //返回association中的value
    return association.autoreleaseReturnedValue();
}

void
_object_set_associative_reference(id object, const void *key, id value, uintptr_t policy)
{
    // This code used to work when nil was passed for object and key. Some code
    // probably relies on that to not crash. Check and handle it explicitly.
    // rdar://problem/44094390
    if (!object && !value) return;

    if (object->getIsa()->forbidsAssociatedObjects())
        _objc_fatal("objc_setAssociatedObject called on instance (%p) of class %s which does not allow associated objects", object, object_getClassName(object));

    //对当前对象的地址做按位取反操作, 就是HashMap的key (哈希函数)
    //根据对象的地址取反 得到HashMap的key - disguised
    DisguisedPtr<objc_object> disguised{(objc_object *)object};
    //创建association对象  里面有两个变量 存储策略和存储的value值
    ObjcAssociation association{policy, value};

    // retain the new value (if any) outside the lock.
    // 获取执行缓存策略之后的 value
    association.acquireValue();

    bool isFirstAssociation = false;
    {
        ///关联对象管理类 - 里面有所有的关联对象 总表
        AssociationsManager manager;
        ///获取关联的HashMap -> 存储当前关联对象 子表
        AssociationsHashMap &associations(manager.get());

        if (value) {
            //返回一个AssociationsHashMap
            /*try_emplace 尝试放置
            
             去AssociationsHashMap 中查找以 disguised为 key 的map
                存在   则返回此map的迭代器
                不存在 则创建map并插入 再返回此map的迭代器
             
             std::pair<iterator, bool>为返回值类型
                first 为迭代器
                second 类型为bool 表示是否是第一次添加
            */
            
            auto refs_result = associations.try_emplace(disguised, ObjectAssociationMap{});
            if (refs_result.second) {
                /* it's the first association we make */
                isFirstAssociation = true;
            }

            /* establish or replace the association */
            /*
             refs    objc::DenseMap<const void *, objc::ObjcAssociation, objc::DenseMapValueInfo<objc::ObjcAssociation>, objc::DenseMapInfo<const void *>, objc::detail::DenseMapPair<const void *, objc::ObjcAssociation> > &    0x000000010066ed28
             refs 是一个map, key为要绑定的属性名, value为 ObjcAssociation对象(存储策略, value)
             */
            auto &refs = refs_result.first->second;
            /*
             尝试向refs 中插入key和association, 原理同上
             */
            auto result = refs.try_emplace(key, std::move(association));
            /*
             如果返回为false, 说明当前key已存在, 将association 交换, 以实现覆盖旧值
             */
            if (!result.second) {
                association.swap(result.first->second);
            }
        } else { //如果value为nil 则说明要将value置空
            
            /*
             先从总表中根据disguised查找ObjectAssociationMap
             再从ObjectAssociationMap中根据key 查找value
             如果找到一个ObjcAssociation
                1.用空值 覆盖原value
                2.从ObjectAssociationMap移除当前association
                3.如果当前ObjectAssociationMap不存在其他的association了, 就从总表AssociationsHashMap中移除出去
                
             */
            auto refs_it = associations.find(disguised);
            if (refs_it != associations.end()) {
                auto &refs = refs_it->second;
                auto it = refs.find(key);
                if (it != refs.end()) {
                    association.swap(it->second);
                    refs.erase(it);
                    if (refs.size() == 0) {
                        associations.erase(refs_it);

                    }
                }
            }
        }
    }

    // Call setHasAssociatedObjects outside the lock, since this
    // will call the object's _noteAssociatedObjects method if it
    // has one, and this may trigger +initialize which might do
    // arbitrary stuff, including setting more associated objects.
    if (isFirstAssociation)
        object->setHasAssociatedObjects();

    // release the old value (outside of the lock).
    /// 最后把之前使用的传入的这个key存储的关联的value释放
    association.releaseHeldValue();
}

// Unlike setting/getting an associated reference,
// this function is performance sensitive because of
// raw isa objects (such as OS Objects) that can't track
// whether they have associated objects.
void
_object_remove_assocations(id object, bool deallocating)
{
    ObjectAssociationMap refs{};

    {
        AssociationsManager manager;
        AssociationsHashMap &associations(manager.get());
        AssociationsHashMap::iterator i = associations.find((objc_object *)object);
        if (i != associations.end()) {
            refs.swap(i->second);

            // If we are not deallocating, then SYSTEM_OBJECT associations are preserved.
            bool didReInsert = false;
            if (!deallocating) {
                for (auto &ref: refs) {
                    if (ref.second.policy() & OBJC_ASSOCIATION_SYSTEM_OBJECT) {
                        i->second.insert(ref);
                        didReInsert = true;
                    }
                }
            }
            if (!didReInsert)
                associations.erase(i);
        }
    }

    // Associations to be released after the normal ones.
    SmallVector<ObjcAssociation *, 4> laterRefs;

    // release everything (outside of the lock).
    for (auto &i: refs) {
        if (i.second.policy() & OBJC_ASSOCIATION_SYSTEM_OBJECT) {
            // If we are not deallocating, then RELEASE_LATER associations don't get released.
            if (deallocating)
                laterRefs.append(&i.second);
        } else {
            i.second.releaseHeldValue();
        }
    }
    for (auto *later: laterRefs) {
        later->releaseHeldValue();
    }
}
