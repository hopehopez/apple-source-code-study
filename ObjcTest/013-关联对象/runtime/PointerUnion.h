/*
 * Copyright (c) 2019 Apple Inc.  All Rights Reserved.
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

#ifndef POINTERUNION_H
#define POINTERUNION_H

#include <cstdint>
#include <atomic>

namespace objc {

template <typename T> struct PointerUnionTypeSelectorReturn {
  using Return = T;
};



/// Get a type based on whether two types are the same or not.
///
/// For:
///
/// \code
///   using Ret = typename PointerUnionTypeSelector<T1, T2, EQ, NE>::Return;
/// \endcode
///
/// Ret will be EQ type if T1 is same as T2 or NE type otherwise.
// 如果 T1 与 T2 相同，则 Ret 为 EQ 类型，否则为 NE 类型。
template <typename T1, typename T2, typename RET_EQ, typename RET_NE>
struct PointerUnionTypeSelector {
  using Return = typename PointerUnionTypeSelectorReturn<RET_NE>::Return;
};

template <typename T, typename RET_EQ, typename RET_NE>
struct PointerUnionTypeSelector<T, T, RET_EQ, RET_NE> {
  using Return = typename PointerUnionTypeSelectorReturn<RET_EQ>::Return;
};

template <typename T1, typename T2, typename RET_EQ, typename RET_NE>
struct PointerUnionTypeSelectorReturn<
    PointerUnionTypeSelector<T1, T2, RET_EQ, RET_NE>> {
  using Return =
      typename PointerUnionTypeSelector<T1, T2, RET_EQ, RET_NE>::Return;
};

//<const class_ro_t, class_rw_ext_t, PTRAUTH_STR("class_ro_t"), PTRAUTH_STR("class_rw_ext_t")
//T1        class_ro_t
//T2        class_rw_ext_t
//Auth1     class_ro_t
//Auth2     class_rw_ext_t
template <class T1, class T2, typename Auth1, typename Auth2>
class PointerUnion {
    // 仅有一个成员变量 _value，
    // 只能保存 const class_ro_t * 或 class_rw_ext_t *
    uintptr_t _value;

    // 两个断言，PT1 和 PT2 内存对齐
    static_assert(alignof(T1) >= 2, "alignment requirement");
    static_assert(alignof(T2) >= 2, "alignment requirement");

    // 定义结构体 IsPT1，内部仅有一个静态不可变 uintptr_t 类型的值为 0 的 Num。
    //（用于 _value 的类型判断, 表示此时是 class_ro_t *）
    struct IsPT1 {
      static const uintptr_t Num = 0;
    };
    
    // 定义结构体 IsPT2，内部仅有一个静态不可变 uintptr_t 类型的值为 1 的 Num。
    //（用于 _value 的类型判断，表示此时是 class_rw_ext_t *）
    struct IsPT2 {
      static const uintptr_t Num = 1;
    };
    template <typename T> struct UNION_DOESNT_CONTAIN_TYPE {};

    // 把 _value 最后一位置为 0 其它位保持不变的值 返回
    uintptr_t getPointer() const {
        return _value & ~1;
    }
    
    // 返回 _value 最后一位的值
    uintptr_t getTag() const {
        return _value & 1;
    }

public:
    // PointerUnion 的构造函数
    // 初始化列表原子操作，初始化 _value
    explicit PointerUnion(const std::atomic<uintptr_t> &raw)
    : _value(raw.load(std::memory_order_relaxed))
    { }
    
    // T1 正常初始化
    PointerUnion(T1 *t, const void *address) {
        _value = (uintptr_t)Auth1::sign(t, address);
    }
    
    // T2 初始化时把 _value 的最后一位置为 1
    PointerUnion(T2 *t, const void *address) {
        _value = (uintptr_t)Auth2::sign(t, address) | 1;
    }

    // 根据指定的 order 以原子方式把 raw 保存到 _value 中
    void storeAt(std::atomic<uintptr_t> &raw, std::memory_order order) const {
        raw.store(_value, order);
    }

    // 极重要的函数，在 class_rw_t 中判断 ro_or_rw_ext 当前是 class_rw_ext_t * 还是 class_ro_t *
    // is 函数在 class_rw_t 中调用时 T 使用的都是 class_rw_ext_t *，当 P1 和 P2 分别对应:
    // const class_ro_t *, class_rw_ext_t * 时，Ty 可以转化为如下:
    // using Ty = typename
    //            PointerUnionTypeSelector<const class_ro_t *,
//                                           class_rw_ext_t *,
//                                           IsPT1,
//
//                                           PointerUnionTypeSelector<class_rw_ext_t *,
//                                                                    class_rw_ext_t *,
//                                                                    IsPT2,
//                                                                    UNION_DOESNT_CONTAIN_TYPE
//                                                                    <class_rw_ext_t *>>
//                                           >::Return;
    // (如果 T1 与 T2 相同，则 Ret 为 EQ 类型，否则为 NE 类型)
    // 如上，PointerUnionTypeSelector 的第四个模版参数 RET_NE 是：
    
    // PointerUnionTypeSelector<class_rw_ext_t *,
//                                class_rw_ext_t *,
//                                IsPT2,
//                                UNION_DOESNT_CONTAIN_TYPE<class_rw_ext_t *>>
                                
    // 然后再执行一次比较，返回是 IsPT2，
    // IsPT2::Num 是 1，
    // getTag() 函数取 _value 第 1 位的值是 1 或者 0，
    // 根据 PointerUnion 的构造函数：PointerUnion(PT2 t) : _value((uintptr_t)t | 1) { }，
    // 可知当 _value 是 class_rw_ext_t * 时，_value 第 1 位是 1，
    // 即当 getTag() == Ty::Num 为真时，表示 _value 是 class_rw_ext_t *

    template <typename T>
    bool is() const {
        using Ty = typename PointerUnionTypeSelector<T1 *,
                                                        T,
                                                    IsPT1,
                                    PointerUnionTypeSelector<T2 *,
                                                                T,
                                                            IsPT2,UNION_DOESNT_CONTAIN_TYPE<T>>
                                                    >::Return;
        return getTag() == Ty::Num;
    }

    // 获取指针 class_ro_t 或者 class_rw_ext_t 指针
    template <typename T> T get(const void *address) const {
        // 确保当前的类型和 T 是匹配的
        ASSERT(is<T>() && "Invalid accessor called");
        
        using AuthT = typename PointerUnionTypeSelector<T1 *, T, Auth1,
            PointerUnionTypeSelector<T2 *, T, Auth2,
            UNION_DOESNT_CONTAIN_TYPE<T>>>::Return;
        
        // getPointer 函数会把 _value 末尾置回 0
        return AuthT::auth((T)getPointer(), address);
    }

    // 几乎同上，但是加了一层判断逻辑，
    // get 函数中如果当前 _value 类型和 T 不匹配的话，强制转换会返回错误类型的指针
    // dyn_cast 则始终都返回 T 类型的指针
    template <typename T> T dyn_cast(const void *address) const {
        // 如果 T 和当前实际类型对应，则直接返回
      if (is<T>())
        return get<T>(address);
        
        // 否则返回 T 类型的值（调用了 T 类型的构造函数）
      return T();
    }
};

template <class PT1, class PT2, class PT3, class PT4 = void>
class PointerUnion4 {
    uintptr_t _value;

    static_assert(alignof(PT1) >= 4, "alignment requirement");
    static_assert(alignof(PT2) >= 4, "alignment requirement");
    static_assert(alignof(PT3) >= 4, "alignment requirement");
    static_assert(alignof(PT4) >= 4, "alignment requirement");

    struct IsPT1 {
      static const uintptr_t Num = 0;
    };
    struct IsPT2 {
      static const uintptr_t Num = 1;
    };
    struct IsPT3 {
      static const uintptr_t Num = 2;
    };
    struct IsPT4 {
      static const uintptr_t Num = 3;
    };
    template <typename T> struct UNION_DOESNT_CONTAIN_TYPE {};

    uintptr_t getPointer() const {
        return _value & ~3;
    }
    uintptr_t getTag() const {
        return _value & 3;
    }

public:
    explicit PointerUnion4(const std::atomic<uintptr_t> &raw)
    : _value(raw.load(std::memory_order_relaxed))
    { }
    PointerUnion4(PT1 t) : _value((uintptr_t)t) { }
    PointerUnion4(PT2 t) : _value((uintptr_t)t | 1) { }
    PointerUnion4(PT3 t) : _value((uintptr_t)t | 2) { }
    PointerUnion4(PT4 t) : _value((uintptr_t)t | 3) { }

    void storeAt(std::atomic<uintptr_t> &raw, std::memory_order order) const {
        raw.store(_value, order);
    }

    template <typename T>
    bool is() const {
        using Ty = typename PointerUnionTypeSelector<PT1, T, IsPT1,
            PointerUnionTypeSelector<PT2, T, IsPT2,
            PointerUnionTypeSelector<PT3, T, IsPT3,
            PointerUnionTypeSelector<PT4, T, IsPT4,
        	UNION_DOESNT_CONTAIN_TYPE<T>>>>>::Return;
        return getTag() == Ty::Num;
    }

    template <typename T> T get() const {
      ASSERT(is<T>() && "Invalid accessor called");
      return reinterpret_cast<T>(getPointer());
    }

    template <typename T> T dyn_cast() const {
      if (is<T>())
        return get<T>();
      return T();
    }
};

} // namespace objc

#endif /* DENSEMAPEXTRAS_H */
