/*
 * Copyright (c) 2005-2007 Apple Inc.  All Rights Reserved.
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

#ifndef _OBJC_RUNTIME_NEW_H
#define _OBJC_RUNTIME_NEW_H

#include "PointerUnion.h"
#include <type_traits>

// class_data_bits_t is the class_t->data field (class_rw_t pointer plus flags)
// The extra bits are optimized for the retain/release and alloc/dealloc paths.

// Values for class_ro_t->flags
// These are emitted by the compiler and are part of the ABI.
// Note: See CGObjCNonFragileABIMac::BuildClassRoTInitializer in clang
// class is a metaclass
#define RO_META               (1<<0)
// class is a root class
#define RO_ROOT               (1<<1)
// class has .cxx_construct/destruct implementations
#define RO_HAS_CXX_STRUCTORS  (1<<2)
// class has +load implementation
// #define RO_HAS_LOAD_METHOD    (1<<3)
// class has visibility=hidden set
#define RO_HIDDEN             (1<<4)
// class has attribute(objc_exception): OBJC_EHTYPE_$_ThisClass is non-weak
#define RO_EXCEPTION          (1<<5)
// class has ro field for Swift metadata initializer callback
#define RO_HAS_SWIFT_INITIALIZER (1<<6)
// class compiled with ARC
#define RO_IS_ARC             (1<<7)
// class has .cxx_destruct but no .cxx_construct (with RO_HAS_CXX_STRUCTORS)
#define RO_HAS_CXX_DTOR_ONLY  (1<<8)
// class is not ARC but has ARC-style weak ivar layout
#define RO_HAS_WEAK_WITHOUT_ARC (1<<9)
// class does not allow associated objects on instances
#define RO_FORBIDS_ASSOCIATED_OBJECTS (1<<10)

// class is in an unloadable bundle - must never be set by compiler
#define RO_FROM_BUNDLE        (1<<29)
// class is unrealized future class - must never be set by compiler
#define RO_FUTURE             (1<<30)
// class is realized - must never be set by compiler
#define RO_REALIZED           (1<<31)

// Values for class_rw_t->flags
// These are not emitted by the compiler and are never used in class_ro_t.
// Their presence should be considered in future ABI versions.
// class_t->data is class_rw_t, not class_ro_t
#define RW_REALIZED           (1<<31)
// class is unresolved future class
#define RW_FUTURE             (1<<30)
// class is initialized
#define RW_INITIALIZED        (1<<29)
// class is initializing
#define RW_INITIALIZING       (1<<28)
// class_rw_t->ro is heap copy of class_ro_t
#define RW_COPIED_RO          (1<<27)
// class allocated but not yet registered
#define RW_CONSTRUCTING       (1<<26)
// class allocated and registered
#define RW_CONSTRUCTED        (1<<25)
// available for use; was RW_FINALIZE_ON_MAIN_THREAD
// #define RW_24 (1<<24)
// class +load has been called
#define RW_LOADED             (1<<23)
#if !SUPPORT_NONPOINTER_ISA
// class instances may have associative references
#define RW_INSTANCES_HAVE_ASSOCIATED_OBJECTS (1<<22)
#endif
// class has instance-specific GC layout
#define RW_HAS_INSTANCE_SPECIFIC_LAYOUT (1 << 21)
// class does not allow associated objects on its instances
#define RW_FORBIDS_ASSOCIATED_OBJECTS       (1<<20)
// class has started realizing but not yet completed it
#define RW_REALIZING          (1<<19)

#if CONFIG_USE_PREOPT_CACHES
// this class and its descendants can't have preopt caches with inlined sels
#define RW_NOPREOPT_SELS      (1<<2)
// this class and its descendants can't have preopt caches
#define RW_NOPREOPT_CACHE     (1<<1)
#endif

// class is a metaclass (copied from ro)
#define RW_META               RO_META // (1<<0)


// NOTE: MORE RW_ FLAGS DEFINED BELOW

// Values for class_rw_t->flags (RW_*), cache_t->_flags (FAST_CACHE_*),
// or class_t->bits (FAST_*).
//
// FAST_* and FAST_CACHE_* are stored on the class, reducing pointer indirection.

#if __LP64__

// class is a Swift class from the pre-stable Swift ABI
#define FAST_IS_SWIFT_LEGACY    (1UL<<0)
// class is a Swift class from the stable Swift ABI
#define FAST_IS_SWIFT_STABLE    (1UL<<1)
// class or superclass has default retain/release/autorelease/retainCount/
//   _tryRetain/_isDeallocating/retainWeakReference/allowsWeakReference
#define FAST_HAS_DEFAULT_RR     (1UL<<2)
// data pointer
#define FAST_DATA_MASK          0x00007ffffffffff8UL

#if __arm64__
// class or superclass has .cxx_construct/.cxx_destruct implementation
//   FAST_CACHE_HAS_CXX_DTOR is the first bit so that setting it in
//   isa_t::has_cxx_dtor is a single bfi
#define FAST_CACHE_HAS_CXX_DTOR       (1<<0)
#define FAST_CACHE_HAS_CXX_CTOR       (1<<1)
// Denormalized RO_META to avoid an indirection
#define FAST_CACHE_META               (1<<2)
#else
// Denormalized RO_META to avoid an indirection
#define FAST_CACHE_META               (1<<0)
// class or superclass has .cxx_construct/.cxx_destruct implementation
//   FAST_CACHE_HAS_CXX_DTOR is chosen to alias with isa_t::has_cxx_dtor
#define FAST_CACHE_HAS_CXX_CTOR       (1<<1)
#define FAST_CACHE_HAS_CXX_DTOR       (1<<2)
#endif

// Fast Alloc fields:
//   This stores the word-aligned size of instances + "ALLOC_DELTA16",
//   or 0 if the instance size doesn't fit.
//
//   These bits occupy the same bits than in the instance size, so that
//   the size can be extracted with a simple mask operation.
//
//   FAST_CACHE_ALLOC_MASK16 allows to extract the instance size rounded
//   rounded up to the next 16 byte boundary, which is a fastpath for
//   _objc_rootAllocWithZone()
#define FAST_CACHE_ALLOC_MASK         0x1ff8
#define FAST_CACHE_ALLOC_MASK16       0x1ff0
#define FAST_CACHE_ALLOC_DELTA16      0x0008

// class's instances requires raw isa
#define FAST_CACHE_REQUIRES_RAW_ISA   (1<<13)
// class or superclass has default alloc/allocWithZone: implementation
// Note this is is stored in the metaclass.
#define FAST_CACHE_HAS_DEFAULT_AWZ    (1<<14)
// class or superclass has default new/self/class/respondsToSelector/isKindOfClass
#define FAST_CACHE_HAS_DEFAULT_CORE   (1<<15)

#else

// class or superclass has .cxx_construct implementation
#define RW_HAS_CXX_CTOR       (1<<18)
// class or superclass has .cxx_destruct implementation
#define RW_HAS_CXX_DTOR       (1<<17)
// class or superclass has default alloc/allocWithZone: implementation
// Note this is is stored in the metaclass.
#define RW_HAS_DEFAULT_AWZ    (1<<16)
// class's instances requires raw isa
#if SUPPORT_NONPOINTER_ISA
#define RW_REQUIRES_RAW_ISA   (1<<15)
#endif
// class or superclass has default retain/release/autorelease/retainCount/
//   _tryRetain/_isDeallocating/retainWeakReference/allowsWeakReference
#define RW_HAS_DEFAULT_RR     (1<<14)
// class or superclass has default new/self/class/respondsToSelector/isKindOfClass
#define RW_HAS_DEFAULT_CORE   (1<<13)

// class is a Swift class from the pre-stable Swift ABI
#define FAST_IS_SWIFT_LEGACY  (1UL<<0)
// class is a Swift class from the stable Swift ABI
#define FAST_IS_SWIFT_STABLE  (1UL<<1)
// data pointer
#define FAST_DATA_MASK        0xfffffffcUL

#endif // __LP64__

// The Swift ABI requires that these bits be defined like this on all platforms.
static_assert(FAST_IS_SWIFT_LEGACY == 1, "resistance is futile");
static_assert(FAST_IS_SWIFT_STABLE == 2, "resistance is futile");


#if __LP64__
typedef uint32_t mask_t;  // x86_64 & arm64 asm are less efficient with 16-bits
#else
typedef uint16_t mask_t;
#endif
typedef uintptr_t SEL;

struct swift_class_t;

enum Atomicity { Atomic = true, NotAtomic = false };
enum IMPEncoding { Encoded = true, Raw = false };

struct bucket_t {
private:
    // IMP-first is better for arm64e ptrauth and no worse for arm64.
    // SEL-first is better for armv7* and i386 and x86_64.
#if __arm64__
    explicit_atomic<uintptr_t> _imp;
    explicit_atomic<SEL> _sel;
#else
    explicit_atomic<SEL> _sel;
    explicit_atomic<uintptr_t> _imp;
#endif

    // Compute the ptrauth signing modifier from &_imp, newSel, and cls.
    uintptr_t modifierForSEL(bucket_t *base, SEL newSel, Class cls) const {
        return (uintptr_t)base ^ (uintptr_t)newSel ^ (uintptr_t)cls;
    }

    // Sign newImp, with &_imp, newSel, and cls as modifiers.
    uintptr_t encodeImp(UNUSED_WITHOUT_PTRAUTH bucket_t *base, IMP newImp, UNUSED_WITHOUT_PTRAUTH SEL newSel, Class cls) const {
        if (!newImp) return 0;
#if CACHE_IMP_ENCODING == CACHE_IMP_ENCODING_PTRAUTH
        return (uintptr_t)
            ptrauth_auth_and_resign(newImp,
                                    ptrauth_key_function_pointer, 0,
                                    ptrauth_key_process_dependent_code,
                                    modifierForSEL(base, newSel, cls));
#elif CACHE_IMP_ENCODING == CACHE_IMP_ENCODING_ISA_XOR
        return (uintptr_t)newImp ^ (uintptr_t)cls;
#elif CACHE_IMP_ENCODING == CACHE_IMP_ENCODING_NONE
        return (uintptr_t)newImp;
#else
#error Unknown method cache IMP encoding.
#endif
    }

public:
    static inline size_t offsetOfSel() { return offsetof(bucket_t, _sel); }
    inline SEL sel() const { return _sel.load(memory_order_relaxed); }

#if CACHE_IMP_ENCODING == CACHE_IMP_ENCODING_ISA_XOR
#define MAYBE_UNUSED_ISA
#else
#define MAYBE_UNUSED_ISA __attribute__((unused))
#endif
    inline IMP rawImp(MAYBE_UNUSED_ISA objc_class *cls) const {
        uintptr_t imp = _imp.load(memory_order_relaxed);
        if (!imp) return nil;
#if CACHE_IMP_ENCODING == CACHE_IMP_ENCODING_PTRAUTH
#elif CACHE_IMP_ENCODING == CACHE_IMP_ENCODING_ISA_XOR
        imp ^= (uintptr_t)cls;
#elif CACHE_IMP_ENCODING == CACHE_IMP_ENCODING_NONE
#else
#error Unknown method cache IMP encoding.
#endif
        return (IMP)imp;
    }

    inline IMP imp(UNUSED_WITHOUT_PTRAUTH bucket_t *base, Class cls) const {
        uintptr_t imp = _imp.load(memory_order_relaxed);
        if (!imp) return nil;
#if CACHE_IMP_ENCODING == CACHE_IMP_ENCODING_PTRAUTH
        SEL sel = _sel.load(memory_order_relaxed);
        return (IMP)
            ptrauth_auth_and_resign((const void *)imp,
                                    ptrauth_key_process_dependent_code,
                                    modifierForSEL(base, sel, cls),
                                    ptrauth_key_function_pointer, 0);
#elif CACHE_IMP_ENCODING == CACHE_IMP_ENCODING_ISA_XOR
        return (IMP)(imp ^ (uintptr_t)cls);
#elif CACHE_IMP_ENCODING == CACHE_IMP_ENCODING_NONE
        return (IMP)imp;
#else
#error Unknown method cache IMP encoding.
#endif
    }

    template <Atomicity, IMPEncoding>
    void set(bucket_t *base, SEL newSel, IMP newImp, Class cls);
};

/* dyld_shared_cache_builder and obj-C agree on these definitions */
enum {
    OBJC_OPT_METHODNAME_START      = 0,
    OBJC_OPT_METHODNAME_END        = 1,
    OBJC_OPT_INLINED_METHODS_START = 2,
    OBJC_OPT_INLINED_METHODS_END   = 3,

    __OBJC_OPT_OFFSETS_COUNT,
};

#if CONFIG_USE_PREOPT_CACHES
extern uintptr_t objc_opt_offsets[__OBJC_OPT_OFFSETS_COUNT];
#endif

/* dyld_shared_cache_builder and obj-C agree on these definitions */
struct preopt_cache_entry_t {
    uint32_t sel_offs;
    uint32_t imp_offs;
};

/* dyld_shared_cache_builder and obj-C agree on these definitions */
struct preopt_cache_t {
    int32_t  fallback_class_offset;
    union {
        struct {
            uint16_t shift       :  5;
            uint16_t mask        : 11;
        };
        uint16_t hash_params;
    };
    uint16_t occupied    : 14;
    uint16_t has_inlines :  1;
    uint16_t bit_one     :  1;
    preopt_cache_entry_t entries[];

    inline int capacity() const {
        return mask + 1;
    }
};

// returns:
// - the cached IMP when one is found
// - nil if there's no cached value and the cache is dynamic
// - `value_on_constant_cache_miss` if there's no cached value and the cache is preoptimized
extern "C" IMP cache_getImp(Class cls, SEL sel, IMP value_on_constant_cache_miss = nil);

struct cache_t {//总体16字节
private:
    explicit_atomic<uintptr_t> _bucketsAndMaybeMask; //是指针，占8字节
    union {
        struct {
            explicit_atomic<mask_t>    _maybeMask;//是mask_t 类型，最多占4字节
#if __LP64__
            uint16_t                   _flags;//是uint16_t类型, 占 2个字节
#endif
            uint16_t                   _occupied;//是uint16_t类型，占 2个字节
        };
        explicit_atomic<preopt_cache_t *> _originalPreoptCache; //是指针，占8字节
    };

#if CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_OUTLINED
    // _bucketsAndMaybeMask is a buckets_t pointer
    // _maybeMask is the buckets mask

    static constexpr uintptr_t bucketsMask = ~0ul;
    static_assert(!CONFIG_USE_PREOPT_CACHES, "preoptimized caches not supported");
#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS
    static constexpr uintptr_t maskShift = 48;
    static constexpr uintptr_t maxMask = ((uintptr_t)1 << (64 - maskShift)) - 1;
    static constexpr uintptr_t bucketsMask = ((uintptr_t)1 << maskShift) - 1;
    
    static_assert(bucketsMask >= MACH_VM_MAX_ADDRESS, "Bucket field doesn't have enough bits for arbitrary pointers.");
#if CONFIG_USE_PREOPT_CACHES
    static constexpr uintptr_t preoptBucketsMarker = 1ul;
    static constexpr uintptr_t preoptBucketsMask = bucketsMask & ~preoptBucketsMarker;
#endif
#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16
    // _bucketsAndMaybeMask is a buckets_t pointer in the low 48 bits
    // _maybeMask is unused, the mask is stored in the top 16 bits.

    // How much the mask is shifted by.
    static constexpr uintptr_t maskShift = 48;

    // Additional bits after the mask which must be zero. msgSend
    // takes advantage of these additional bits to construct the value
    // `mask << 4` from `_maskAndBuckets` in a single instruction.
    static constexpr uintptr_t maskZeroBits = 4;

    // The largest mask value we can store.
    static constexpr uintptr_t maxMask = ((uintptr_t)1 << (64 - maskShift)) - 1;
    
    // The mask applied to `_maskAndBuckets` to retrieve the buckets pointer.
    static constexpr uintptr_t bucketsMask = ((uintptr_t)1 << (maskShift - maskZeroBits)) - 1;
    
    // Ensure we have enough bits for the buckets pointer.
    static_assert(bucketsMask >= MACH_VM_MAX_ADDRESS,
            "Bucket field doesn't have enough bits for arbitrary pointers.");

#if CONFIG_USE_PREOPT_CACHES
    static constexpr uintptr_t preoptBucketsMarker = 1ul;
#if __has_feature(ptrauth_calls)
    // 63..60: hash_mask_shift
    // 59..55: hash_shift
    // 54.. 1: buckets ptr + auth
    //      0: always 1
    static constexpr uintptr_t preoptBucketsMask = 0x007ffffffffffffe;
    static inline uintptr_t preoptBucketsHashParams(const preopt_cache_t *cache) {
        uintptr_t value = (uintptr_t)cache->shift << 55;
        // masks have 11 bits but can be 0, so we compute
        // the right shift for 0x7fff rather than 0xffff
        return value | ((objc::mask16ShiftBits(cache->mask) - 1) << 60);
    }
#else
    // 63..53: hash_mask
    // 52..48: hash_shift
    // 47.. 1: buckets ptr
    //      0: always 1
    static constexpr uintptr_t preoptBucketsMask = 0x0000fffffffffffe;
    static inline uintptr_t preoptBucketsHashParams(const preopt_cache_t *cache) {
        return (uintptr_t)cache->hash_params << 48;
    }
#endif
#endif // CONFIG_USE_PREOPT_CACHES
#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_LOW_4
    // _bucketsAndMaybeMask is a buckets_t pointer in the top 28 bits
    // _maybeMask is unused, the mask length is stored in the low 4 bits

    static constexpr uintptr_t maskBits = 4;
    static constexpr uintptr_t maskMask = (1 << maskBits) - 1;
    static constexpr uintptr_t bucketsMask = ~maskMask;
    static_assert(!CONFIG_USE_PREOPT_CACHES, "preoptimized caches not supported");
#else
#error Unknown cache mask storage type.
#endif

    bool isConstantEmptyCache() const;
    bool canBeFreed() const;
    mask_t mask() const;

#if CONFIG_USE_PREOPT_CACHES
    void initializeToPreoptCacheInDisguise(const preopt_cache_t *cache);
    const preopt_cache_t *disguised_preopt_cache() const;
#endif

    void incrementOccupied();
    void setBucketsAndMask(struct bucket_t *newBuckets, mask_t newMask);

    void reallocate(mask_t oldCapacity, mask_t newCapacity, bool freeOld);
    void collect_free(bucket_t *oldBuckets, mask_t oldCapacity);

    static bucket_t *emptyBuckets();
    static bucket_t *allocateBuckets(mask_t newCapacity);
    static bucket_t *emptyBucketsForCapacity(mask_t capacity, bool allocate = true);
    static struct bucket_t * endMarker(struct bucket_t *b, uint32_t cap);
    void bad_cache(id receiver, SEL sel) __attribute__((noreturn, cold));

public:
    // The following four fields are public for objcdt's use only.
    // objcdt reaches into fields while the process is suspended
    // hence doesn't care for locks and pesky little details like this
    // and can safely use these.
    unsigned capacity() const;
    struct bucket_t *buckets() const;
    Class cls() const;

#if CONFIG_USE_PREOPT_CACHES
    const preopt_cache_t *preopt_cache() const;
#endif

    mask_t occupied() const;
    void initializeToEmpty();

#if CONFIG_USE_PREOPT_CACHES
    bool isConstantOptimizedCache(bool strict = false, uintptr_t empty_addr = (uintptr_t)&_objc_empty_cache) const;
    bool shouldFlush(SEL sel, IMP imp) const;
    bool isConstantOptimizedCacheWithInlinedSels() const;
    Class preoptFallbackClass() const;
    void maybeConvertToPreoptimized();
    void initializeToEmptyOrPreoptimizedInDisguise();
#else
    inline bool isConstantOptimizedCache(bool strict = false, uintptr_t empty_addr = 0) const { return false; }
    inline bool shouldFlush(SEL sel, IMP imp) const {
        return cache_getImp(cls(), sel) == imp;
    }
    inline bool isConstantOptimizedCacheWithInlinedSels() const { return false; }
    inline void initializeToEmptyOrPreoptimizedInDisguise() { initializeToEmpty(); }
#endif

    void insert(SEL sel, IMP imp, id receiver);
    void copyCacheNolock(objc_imp_cache_entry *buffer, int len);
    void destroy();
    void eraseNolock(const char *func);

    static void init();
    static void collectNolock(bool collectALot);
    static size_t bytesForCapacity(uint32_t cap);

#if __LP64__
    bool getBit(uint16_t flags) const {
        return _flags & flags;
    }
    void setBit(uint16_t set) {
        __c11_atomic_fetch_or((_Atomic(uint16_t) *)&_flags, set, __ATOMIC_RELAXED);
    }
    void clearBit(uint16_t clear) {
        __c11_atomic_fetch_and((_Atomic(uint16_t) *)&_flags, ~clear, __ATOMIC_RELAXED);
    }
#endif

#if FAST_CACHE_ALLOC_MASK
    bool hasFastInstanceSize(size_t extra) const
    {
        if (__builtin_constant_p(extra) && extra == 0) {
            return _flags & FAST_CACHE_ALLOC_MASK16;
        }
        return _flags & FAST_CACHE_ALLOC_MASK;
    }

    size_t fastInstanceSize(size_t extra) const
    {
        ASSERT(hasFastInstanceSize(extra));

        if (__builtin_constant_p(extra) && extra == 0) {
            return _flags & FAST_CACHE_ALLOC_MASK16;
        } else {
            size_t size = _flags & FAST_CACHE_ALLOC_MASK;
            // remove the FAST_CACHE_ALLOC_DELTA16 that was added
            // by setFastInstanceSize
            return align16(size + extra - FAST_CACHE_ALLOC_DELTA16);
        }
    }

    void setFastInstanceSize(size_t newSize)
    {
        // Set during realization or construction only. No locking needed.
        uint16_t newBits = _flags & ~FAST_CACHE_ALLOC_MASK;
        uint16_t sizeBits;

        // Adding FAST_CACHE_ALLOC_DELTA16 allows for FAST_CACHE_ALLOC_MASK16
        // to yield the proper 16byte aligned allocation size with a single mask
        sizeBits = word_align(newSize) + FAST_CACHE_ALLOC_DELTA16;
        sizeBits &= FAST_CACHE_ALLOC_MASK;
        if (newSize <= sizeBits) {
            newBits |= sizeBits;
        }
        _flags = newBits;
    }
#else
    bool hasFastInstanceSize(size_t extra) const {
        return false;
    }
    size_t fastInstanceSize(size_t extra) const {
        abort();
    }
    void setFastInstanceSize(size_t extra) {
        // nothing
    }
#endif
};


// classref_t is unremapped class_t*
typedef struct classref * classref_t;


/***********************************************************************
* RelativePointer<T>
* A pointer stored as an offset from the address of that offset.
*
* The target address is computed by taking the address of this struct
* and adding the offset stored within it. This is a 32-bit signed
* offset giving ±2GB of range.
**********************************************************************/
template <typename T>
struct RelativePointer: nocopy_t {
    int32_t offset;

    T get() const {
        if (offset == 0)
            return nullptr;
        uintptr_t base = (uintptr_t)&offset;
        uintptr_t signExtendedOffset = (uintptr_t)(intptr_t)offset;
        uintptr_t pointer = base + signExtendedOffset;
        return (T)pointer;
    }
};


#ifdef __PTRAUTH_INTRINSICS__
#   define StubClassInitializerPtrauth __ptrauth(ptrauth_key_function_pointer, 1, 0xc671)
#else
#   define StubClassInitializerPtrauth
#endif
struct stub_class_t {
    uintptr_t isa;
    _objc_swiftMetadataInitializer StubClassInitializerPtrauth initializer;
};

// A pointer modifier that does nothing to the pointer.
struct PointerModifierNop {
    template <typename ListType, typename T>
    static T *modify(__unused const ListType &list, T *ptr) { return ptr; }
};

/***********************************************************************
* entsize_list_tt<Element, List, FlagMask, PointerModifier>
* Generic implementation of an array of non-fragile structs.
  non-fragile 结构体数组的通用实现。
*
* Element is the struct type (e.g. method_t)
 Element 是结构体类型，如: method_t
* List is the specialization of entsize_list_tt (e.g. method_list_t)
 List 是 entsize_list_tt 指定类型，如: method_list_t
* FlagMask is used to stash extra bits in the entsize field
*   (e.g. method list fixup markers)
 FlagMask 用于在 entsize 字段中存储多余的位。
 如: 方法列表被修改的标记位
* PointerModifier is applied to the element pointers retrieved from
* the array.
 PointerModifier 数组元素指针
**********************************************************************/
template <typename Element, typename List, uint32_t FlagMask, typename PointerModifier = PointerModifierNop>
//它可理解为一个数据容器，拥有自己的迭代器用于遍历元素
struct entsize_list_tt {
    // entsize（entry 的大小） 和 Flags 以掩码形式保存在 entsizeAndFlags 中
    uint32_t entsizeAndFlags;
    
    // entsize_list_tt 的容量
    uint32_t count;


    // method_list_t 定义中 FlagMask 的值是: 0x3
    // ivar_list_t 定义中 FlagMask 的值是: 0
    // property_list_t 定义中 FlagMask 的值是: 0
    
    // 元素的大小（entry 的大小）
    uint32_t entsize() const {
        return entsizeAndFlags & ~FlagMask;
    }
    
    // 从  entsizeAndFlags 中取出 flags
    uint32_t flags() const {
        return entsizeAndFlags & FlagMask;
    }

    // 返回指定索引的元素的的引用，orEnd 表示 i 可以等于 count，
    // 当 i 等于 count 时返回最后一个元素的后面的位置。
    Element& getOrEnd(uint32_t i) const {
        // 断言，i 不能超过 count
        ASSERT(i <= count);
        // 首先取出 this 地址（强转为 uint8_t 指针），然后指针偏移sizeof(*this)个字节长度, 然后再指针偏移 i * ensize() 个字节长度，
        // 然后转换为 Element 指针，然后解引用取出指针指向内容作为 Element 引用返回。
        return *PointerModifier::modify(*this, (Element *)((uint8_t *)this + sizeof(*this) + i*entsize()));
    }
    
    // 在索引范围内返回 Element 引用
    Element& get(uint32_t i) const { 
        ASSERT(i < count);
        return getOrEnd(i);
    }

    // entsize_list_tt 占用的内存总大小，以字节为单位
    size_t byteSize() const {
        return byteSize(entsize(), count);
    }
    
    // entsize_list_tt 占用的内存总大小，以字节为单位
    // entsize 单个元素的内存大小，count 是总的元素的个数
    static size_t byteSize(uint32_t entsize, uint32_t count) {
        // 首先算出 struct entsize_list_tt 的内存大小，
        // 即 uint32_t entsizeAndFlags + uint32_t count
        // 两个成员变量的总长度，然后加上元素数组的总的内存大小。
        return sizeof(entsize_list_tt) + count*entsize;
    }

    // 自定义的迭代器的声明，实现在下面
    struct iterator;
    
    // 起始位置的迭代器
    const iterator begin() const {
        // static_cast 是一个 c++ 运算符，功能是把一个表达式转换为某种类型，
          // 但没有运行时类型检查来保证转换的安全性。
          // 把 this 强制转换为 const List *
          // 0 对应下面 iterator 的构造函数实现可知，
          // 把 element 指向第 1 个元素
          
          // 即返回指向容器第一个元素的迭代器
        return iterator(*static_cast<const List*>(this), 0); 
    }
    // 同上，少了两个 const 修饰，前面的 const 表示函数返回值为 const 不可变
    // 后面的 const 表示函数执行过程中不改变原始对象里的内容
    iterator begin() { 
        return iterator(*static_cast<const List*>(this), 0); 
    }
    
    // 返回指向 entsize_list_tt 最后一个元素的后面的迭代器，
    // 注意这里不是指向最后一个元素，而是指向最后一个的后面。
    const iterator end() const { 
        return iterator(*static_cast<const List*>(this), count); 
    }
    // 同上，去掉了两个 const 限制
    iterator end() { 
        return iterator(*static_cast<const List*>(this), count); 
    }

    // entsize_list_tt 的自定义的迭代器实现
    struct iterator {
        // 元素（entry）的大小
        uint32_t entsize;
        
        // 当前迭代器对应的索引
        uint32_t index;  // keeping track of this saves a divide in operator-
        
        // 当前迭代器对应的元素指针
        Element* element;

        // 声明类型别名
        typedef std::random_access_iterator_tag iterator_category;
        typedef Element value_type;
        typedef ptrdiff_t difference_type;
        typedef Element* pointer;
        typedef Element& reference;

        // 构造函数
        iterator() { }

        // 构造函数
        // start 默认值是 0 即 index 默认从 0 开始，element 默认指向第一个元素
        iterator(const List& list, uint32_t start = 0)
            : entsize(list.entsize())
            , index(start)
            , element(&list.getOrEnd(start))
        { }

        // 下面是一系列重载操作符
        // +=
        const iterator& operator += (ptrdiff_t delta) {
            // 指针向后偏移
            element = (Element*)((uint8_t *)element + delta*entsize);
            // 更新 index
            index += (int32_t)delta;
            // 返回 *this
            return *this;
        }
        // -=
        const iterator& operator -= (ptrdiff_t delta) {
            // 指针向前偏移
            element = (Element*)((uint8_t *)element - delta*entsize);
            // 更新 index
            index -= (int32_t)delta;
            // 返回 *this
            return *this;
        }
        
        // 以下都是 += 和 -= 的应用
        // +
        const iterator operator + (ptrdiff_t delta) const {
            return iterator(*this) += delta;
        }
        // -
        const iterator operator - (ptrdiff_t delta) const {
            return iterator(*this) -= delta;
        }

        // ++
        iterator& operator ++ () { *this += 1; return *this; }
        // --
        iterator& operator -- () { *this -= 1; return *this; }
        // ++（int）
        iterator operator ++ (int) {
            iterator result(*this); *this += 1; return result;
        }
        // --（int）
        iterator operator -- (int) {
            iterator result(*this); *this -= 1; return result;
        }

        // 两个迭代器的之间的距离
        ptrdiff_t operator - (const iterator& rhs) const {
            return (ptrdiff_t)this->index - (ptrdiff_t)rhs.index;
        }

        // 返回元素指针或引用
        Element& operator * () const { return *element; }
        Element* operator -> () const { return element; }

        operator Element& () const { return *element; }

        // 判等，看到的是直接比较 element 的地址
        // 哦哦，不一定是直接的地址比较，== 可能被模版抽象类型 Element 重载
        bool operator == (const iterator& rhs) const {
            return this->element == rhs.element;
        }
        // 不等
        bool operator != (const iterator& rhs) const {
            return this->element != rhs.element;
        }

        // 大概是前后位置比较
        // 小于
        bool operator < (const iterator& rhs) const {
            return this->element < rhs.element;
        }
        // 大于
        bool operator > (const iterator& rhs) const {
            return this->element > rhs.element;
        }
    };
};


namespace objc {
// Let method_t::small use this from objc-private.h.
static inline bool inSharedCache(uintptr_t ptr);
}

struct method_t {
    static const uint32_t smallMethodListFlag = 0x80000000;

    method_t(const method_t &other) = delete;

    // The representation of a "big" method. This is the traditional
    // representation of three pointers storing the selector, types
    // and implementation.
    struct big {
        SEL name; // 方法名
        const char *types; // 方法类型
        MethodListIMP imp; // 方法实现
    };

private:
    bool isSmall() const {
        return ((uintptr_t)this & 1) == 1;
    }

    // The representation of a "small" method. This stores three
    // relative offsets to the name, types, and implementation.
    struct small {
        // The name field either refers to a selector (in the shared
        // cache) or a selref (everywhere else).
        RelativePointer<const void *> name;
        RelativePointer<const char *> types;
        RelativePointer<IMP> imp;

        bool inSharedCache() const {
            return (CONFIG_SHARED_CACHE_RELATIVE_DIRECT_SELECTORS &&
                    objc::inSharedCache((uintptr_t)this));
        }
    };

    small &small() const {
        ASSERT(isSmall());
        return *(struct small *)((uintptr_t)this & ~(uintptr_t)1);
    }

    IMP remappedImp(bool needsLock) const;
    void remapImp(IMP imp);
    objc_method_description *getSmallDescription() const;

public:
    static const auto bigSize = sizeof(struct big);
    static const auto smallSize = sizeof(struct small);

    // The pointer modifier used with method lists. When the method
    // list contains small methods, set the bottom bit of the pointer.
    // We use that bottom bit elsewhere to distinguish between big
    // and small methods.
    struct pointer_modifier {
        template <typename ListType>
        static method_t *modify(const ListType &list, method_t *ptr) {
            if (list.flags() & smallMethodListFlag)
                return (method_t *)((uintptr_t)ptr | 1);
            return ptr;
        }
    };

    big &big() const {
        ASSERT(!isSmall());
        return *(struct big *)this;
    }

    SEL name() const {
        if (isSmall()) {
            return (small().inSharedCache()
                    ? (SEL)small().name.get()
                    : *(SEL *)small().name.get());
        } else {
            return big().name;
        }
    }
    const char *types() const {
        return isSmall() ? small().types.get() : big().types;
    }
    IMP imp(bool needsLock) const {
        if (isSmall()) {
            IMP imp = remappedImp(needsLock);
            if (!imp)
                imp = ptrauth_sign_unauthenticated(small().imp.get(),
                                                   ptrauth_key_function_pointer, 0);
            return imp;
        }
        return big().imp;
    }

    SEL getSmallNameAsSEL() const {
        ASSERT(small().inSharedCache());
        return (SEL)small().name.get();
    }

    SEL getSmallNameAsSELRef() const {
        ASSERT(!small().inSharedCache());
        return *(SEL *)small().name.get();
    }

    void setName(SEL name) {
        if (isSmall()) {
            ASSERT(!small().inSharedCache());
            *(SEL *)small().name.get() = name;
        } else {
            big().name = name;
        }
    }

    void setImp(IMP imp) {
        if (isSmall()) {
            remapImp(imp);
        } else {
            big().imp = imp;
        }
    }

    objc_method_description *getDescription() const {
        return isSmall() ? getSmallDescription() : (struct objc_method_description *)this;
    }

    // 根据选择子的地址进行排序
    struct SortBySELAddress :
    public std::binary_function<const struct method_t::big&,
                                const struct method_t::big&, bool>
    {
        bool operator() (const struct method_t::big& lhs,
                         const struct method_t::big& rhs)
        { return lhs.name < rhs.name; }
    };

    method_t &operator=(const method_t &other) {
        ASSERT(!isSmall());
        big().name = other.name();
        big().types = other.types();
        big().imp = other.imp(false);
        return *this;
    }
};

struct ivar_t {
#if __x86_64__
    // *offset was originally 64-bit on some x86_64 platforms.
    // *offset 最初在某些 x86_64 平台上为 64 位。
    // We read and write only 32 bits of it.
    // 我们只读取和写入 32 位
    // Some metadata provides all 64 bits. This is harmless for unsigned 
    // little-endian values.
    // 一些元数据提供所有 64 位。这对于无符号的 Little-endian (小端存储) 值无害。
    
    // Some code uses all 64 bits. class_addIvar() over-allocates the 
    // offset for their benefit.
    // 一些代码使用所有 64 位。class_addIvar() 为他们的利益过度分配了偏移量。
#endif
    // 首先这里要和结构体中成员变量的偏移距离做出理解上的区别。

    // offset 它是一个指针，那它指向谁呢，它指向一个全局变量，
    // 编译器为每个类的每个成员变量都分配了一个全局变量，用于存储该成员变量的偏移值。

    // 如果 offset 仅是成员变量偏移距离的一个整数的话，能想到的成员变量的读取可能是这样的：
    // 当我们读取成员变量时从实例对象的 isa 找到类，然后 data() 找到 class_rw_t
    // 然后在找到 class_ro_t 然后再找到 ivar_list_t，
    // 然后再比较 ivar_t 的 name 和我们要访问的成员变量的名字是否相同，然后再读出 int 类型的 offset,
    // 再进行 self + offset 指针偏移找到这个成员变量。

    // 现在当我们访问一个成员变量时，只需要 self + *offset 就可以了。
    // 下面会单独讲解 offset 指针。

    int32_t *offset;
    
    // 成员变量名称（如果类中有属性的话，编译器会自动生成 _属性名 的成员变量）
    const char *name;
    
    // 成员变量类型
    const char *type;
    
    // alignment is sometimes -1; use alignment() instead
    // 对齐有时为 -1，使用 alignment() 代替。
    
    // 原始对齐值是 2^alignment_raw 的值
    // alignment_raw 应该叫做对齐值的指数
    uint32_t alignment_raw;
    
    // 成员变量的类型的大小
    uint32_t size;

    /*
     #ifdef __LP64__
     #   define WORD_SHIFT 3UL
     #   define WORD_MASK 7UL
     #   define WORD_BITS 64
     #else
     #   define WORD_SHIFT 2UL
     #   define WORD_MASK 3UL
     #   define WORD_BITS 32
     #endif
     */
    uint32_t alignment() const {
        // 应该没有类型的 alignment_raw 会是 uint32_t 类型的最大值吧！
        // WORD_SHIFT 在 __LP64__ 下是 3，表示 8 字节对齐，
        // 在非 __LP64__ 下是 2^2 = 4 字节对齐。
        if (alignment_raw == ~(uint32_t)0) return 1U << WORD_SHIFT;
        
        // 2^alignment_raw
        return 1 << alignment_raw;
    }
};

struct property_t {
    // 属性名字
    const char *name;
    
    // 属性的 attributes，例如：
        
    // @property (nonatomic, strong) NSObject *object;
    // object 的 attributes："T@\"NSObject\",&,N,V_object"
    //
    // @property (nonatomic, copy) NSArray *array2;
    // array2 的 attributes："T@\"NSArray\",C,N,V_array2"
    //

    const char *attributes;
};

// Two bits of entsize are used for fixup markers.
// entsize 的后两位用于固定标记。

// Reserve the top half of entsize for more flags. We never
// need entry sizes anywhere close to 64kB.
//
// Currently there is one flag defined: the small method list flag,
// method_t::smallMethodListFlag. Other flags are currently ignored.
// (NOTE: these bits are only ignored on runtimes that support small
// method lists. Older runtimes will treat them as part of the entry
// size!)
struct method_list_t : entsize_list_tt<method_t, method_list_t, 0xffff0003, method_t::pointer_modifier> {
    bool isUniqued() const;
    bool isFixedUp() const;
    void setFixedUp();

    uint32_t indexOfMethod(const method_t *meth) const {
        uint32_t i = 
            (uint32_t)(((uintptr_t)meth - (uintptr_t)this) / entsize());
        ASSERT(i < count);
        return i;
    }

    bool isSmallList() const {
        return flags() & method_t::smallMethodListFlag;
    }

    bool isExpectedSize() const {
        if (isSmallList())
            return entsize() == method_t::smallSize;
        else
            return entsize() == method_t::bigSize;
    }

    method_list_t *duplicate() const {
        method_list_t *dup;
        if (isSmallList()) {
            dup = (method_list_t *)calloc(byteSize(method_t::bigSize, count), 1);
            dup->entsizeAndFlags = method_t::bigSize;
        } else {
            dup = (method_list_t *)calloc(this->byteSize(), 1);
            dup->entsizeAndFlags = this->entsizeAndFlags;
        }
        dup->count = this->count;
        std::copy(begin(), end(), dup->begin());
        return dup;
    }
};

struct ivar_list_t : entsize_list_tt<ivar_t, ivar_list_t, 0> {
    bool containsIvar(Ivar ivar) const {
        // 地址比较
        return (ivar >= (Ivar)&*begin()  &&  ivar < (Ivar)&*end());
    }
};

struct property_list_t : entsize_list_tt<property_t, property_list_t, 0> {
};


typedef uintptr_t protocol_ref_t;  // protocol_t *, but unremapped

// Values for protocol_t->flags
#define PROTOCOL_FIXED_UP_2     (1<<31)  // must never be set by compiler
#define PROTOCOL_FIXED_UP_1     (1<<30)  // must never be set by compiler
#define PROTOCOL_IS_CANONICAL   (1<<29)  // must never be set by compiler
// Bits 0..15 are reserved for Swift's use.

#define PROTOCOL_FIXED_UP_MASK (PROTOCOL_FIXED_UP_1 | PROTOCOL_FIXED_UP_2)

struct protocol_t : objc_object {
    //协议名
    const char *mangledName;
    // 所属的还是所继承的协议
    // 例如定义了一个：
    // @protocol CustomProtocol <NSObject>
    // ...
    // @end
    // 然后通过层层打印发现 CustomProtocol 的 protocols 是 NSObject
    struct protocol_list_t *protocols;

    // 协议中的实例方法
    method_list_t *instanceMethods;
    // 协议中的类方法
    method_list_t *classMethods;
    // 协议中的可选的实例方法
    method_list_t *optionalInstanceMethods;
    // 协议中可选的类方法
    method_list_t *optionalClassMethods;
    // 协议中声明的属性
    property_list_t *instanceProperties;
    
    // 这个 size 大概是整个 protocol_t 内容大小
    // 协议中声明了 一个实例方法一个类方法
    // 一个可选的实例方法一个可选的类方法，外加一个 NSObject 类型的属性
    // 打印时，size 的值是 96
    uint32_t size;   // sizeof(protocol_t)
    uint32_t flags;
    
    
    // Fields below this point are not always present on disk.
    // 低于此点的字段并不总是存在于磁盘上。
    
    const char **_extendedMethodTypes;
    const char *_demangledName;
    property_list_t *_classProperties;

    const char *demangledName();

    // 协议名
    const char *nameForLogging() {
        return demangledName();
    }

    // 下面一部分内容等详细分析 protocol 时再解析。
    bool isFixedUp() const;
    void setFixedUp();

    bool isCanonical() const;
    void clearIsCanonical();

#   define HAS_FIELD(f) ((uintptr_t)(&f) < ((uintptr_t)this + size))

    bool hasExtendedMethodTypesField() const {
        return HAS_FIELD(_extendedMethodTypes);
    }
    bool hasDemangledNameField() const {
        return HAS_FIELD(_demangledName);
    }
    bool hasClassPropertiesField() const {
        return HAS_FIELD(_classProperties);
    }

#   undef HAS_FIELD

    const char **extendedMethodTypes() const {
        return hasExtendedMethodTypesField() ? _extendedMethodTypes : nil;
    }

    property_list_t *classProperties() const {
        return hasClassPropertiesField() ? _classProperties : nil;
    }
};

struct protocol_list_t {
    // count is pointer-sized by accident.
    // list 的长度
    uintptr_t count;
    
    // 实际是 protocol_t 数组
    protocol_ref_t list[0]; // variable-size

    // protocol_list_t 所占用的内存大小
    size_t byteSize() const {
        return sizeof(*this) + count*sizeof(list[0]);
    }

    // 完全的内存空间进行复制
    protocol_list_t *duplicate() const {
        return (protocol_list_t *)memdup(this, this->byteSize());
    }

    // 迭代器和 const 迭代器
    typedef protocol_ref_t* iterator;
    typedef const protocol_ref_t* const_iterator;

    // 后面的 const 表示不会改变 protocol_list_t 内的数据
    const_iterator begin() const {
        return list;
    }
    iterator begin() {
        return list;
    }
    
    // list 最后一个元素的后面。
    const_iterator end() const {
        return list + count;
    }
    iterator end() {
        return list + count;
    }
};

struct class_ro_t {
    // 通过掩码保存的一些标志位
    
    /*
    // class is a metaclass
    #define RO_META               (1<<0)
    // class is a root class
    #define RO_ROOT               (1<<1)
    // class has .cxx_construct/destruct implementations
    #define RO_HAS_CXX_STRUCTORS  (1<<2)
    // class has +load implementation
    // #define RO_HAS_LOAD_METHOD    (1<<3)
    // class has visibility=hidden set
    #define RO_HIDDEN             (1<<4)
    // class has attribute(objc_exception): OBJC_EHTYPE_$_ThisClass is non-weak
    #define RO_EXCEPTION          (1<<5)
    // class has ro field for Swift metadata initializer callback
    #define RO_HAS_SWIFT_INITIALIZER (1<<6)
    // class compiled with ARC
    #define RO_IS_ARC             (1<<7)
    // class has .cxx_destruct but no .cxx_construct (with RO_HAS_CXX_STRUCTORS)
    #define RO_HAS_CXX_DTOR_ONLY  (1<<8)
    // class is not ARC but has ARC-style weak ivar layout
    #define RO_HAS_WEAK_WITHOUT_ARC (1<<9)
    // class does not allow associated objects on instances
    #define RO_FORBIDS_ASSOCIATED_OBJECTS (1<<10)

    // class is in an unloadable bundle - must never be set by compiler
    #define RO_FROM_BUNDLE        (1<<29)
    // class is unrealized future class - must never be set by compiler
    #define RO_FUTURE             (1<<30)
    // class is realized - must never be set by compiler
    #define RO_REALIZED           (1<<31)
     */
    uint32_t flags;
    
    // 自己成员变量的起始偏移量
    // 因为会继承父类的成员变量, 所以自己的成员变量,要排在父类的成员变量之后
    uint32_t instanceStart;
    
    // 根据内存对齐计算成员变量从前到后所占用的内存大小，
    // 不过没有进行总体的内存对齐，例如最后一个成员变量是 char 时，
    // 则最后只是加 1，instanceSize 的值是一个奇数，
    // 再进行一个整体 8/4 字节对齐就好了，
    //（__LP64__ 平台下 8 字节对齐，其它则是 4 字节对齐）
        
    // objc_class 的 alignedInstanceSize 函数，
    // 完成了这最后一步的整体内存对齐。
    
    // 尚未进行内存对齐的实例大小
    uint32_t instanceSize;
#ifdef __LP64__
    //仅在 64 位系统架构下的包含的保留位
    uint32_t reserved;
#endif

    union {
        // 记录了哪些是 strong 的 ivar
        const uint8_t * ivarLayout;
        // 元类
        Class nonMetaclass;
    };

    // name 应该是类名
    explicit_atomic<const char *> name;
    // With ptrauth, this is signed if it points to a small list, but
    // may be unsigned if it points to a big list.
    
    // 实例方法列表
    void *baseMethodList;
    // 协议列表
    protocol_list_t * baseProtocols;
    // 成员变量列表
    const ivar_list_t * ivars;

    // 记录了哪些是 weak 的 ivar
    const uint8_t * weakIvarLayout;
    // 属性列表
    property_list_t *baseProperties;

    // This field exists only when RO_HAS_SWIFT_INITIALIZER is set.
    // 仅当设置了 RO_HAS_SWIFT_INITIALIZER 时，此字段才存在。
    // 回调函数指针数组
    // 从 Objective-C 回调 Swift，以执行 Swift 类初始化
    _objc_swiftMetadataInitializer __ptrauth_objc_method_list_imp _swiftMetadataInitializer_NEVER_USE[0];

    // 返回 0 索引大概是用于返回起始地址
    _objc_swiftMetadataInitializer swiftMetadataInitializer() const {
        // class has ro field for Swift metadata initializer callback.
        // 类具有用于 Swift 元数据初始化程序回调的 ro 字段。
        // #define RO_HAS_SWIFT_INITIALIZER (1<<6) // flags 的第 6 位是一个标识位

        if (flags & RO_HAS_SWIFT_INITIALIZER) {
            return _swiftMetadataInitializer_NEVER_USE[0];
        } else {
            return nil;
        }
    }

    const char *getName() const {
        return name.load(std::memory_order_acquire);
    }

    static const uint16_t methodListPointerDiscriminator = 0xC310;
#if 0 // FIXME: enable this when we get a non-empty definition of __ptrauth_objc_method_list_pointer from ptrauth.h.
        static_assert(std::is_same<
                      void * __ptrauth_objc_method_list_pointer *,
                      void * __ptrauth(ptrauth_key_method_list_pointer, 1, methodListPointerDiscriminator) *>::value,
                      "Method list pointer signing discriminator must match ptrauth.h");
#endif

    // 返回方法列表
    method_list_t *baseMethods() const {
#if __has_feature(ptrauth_calls)
        method_list_t *ptr = ptrauth_strip((method_list_t *)baseMethodList, ptrauth_key_method_list_pointer);
        if (ptr == nullptr)
            return nullptr;

        // Don't auth if the class_ro and the method list are both in the shared cache.
        // This is secure since they'll be read-only, and this allows the shared cache
        // to cut down on the number of signed pointers it has.
        bool roInSharedCache = objc::inSharedCache((uintptr_t)this);
        bool listInSharedCache = objc::inSharedCache((uintptr_t)ptr);
        if (roInSharedCache && listInSharedCache)
            return ptr;

        // Auth all other small lists.
        if (ptr->isSmallList())
            ptr = ptrauth_auth_data((method_list_t *)baseMethodList,
                                    ptrauth_key_method_list_pointer,
                                    ptrauth_blend_discriminator(&baseMethodList,
                                                                methodListPointerDiscriminator));
        return ptr;
#else
        return (method_list_t *)baseMethodList;
#endif
    }

    uintptr_t baseMethodListPtrauthData() const {
        return ptrauth_blend_discriminator(&baseMethodList,
                                           methodListPointerDiscriminator);
    }

    // 完全复制一份 class_ro_t
    class_ro_t *duplicate() const {
        // 类具有用于 Swift 元数据初始化程序回调的 ro 字段。
        // #define RO_HAS_SWIFT_INITIALIZER (1<<6)
        bool hasSwiftInitializer = flags & RO_HAS_SWIFT_INITIALIZER;

        size_t size = sizeof(*this);
        if (hasSwiftInitializer)
            // 附加 _swiftMetadataInitializer_NEVER_USE 数组的赋值
            size += sizeof(_swiftMetadataInitializer_NEVER_USE[0]);

        
        class_ro_t *ro = (class_ro_t *)memdup(this, size);

        if (hasSwiftInitializer)
            ro->_swiftMetadataInitializer_NEVER_USE[0] = this->_swiftMetadataInitializer_NEVER_USE[0];

#if __has_feature(ptrauth_calls)
        // Re-sign the method list pointer if it was signed.
        // NOTE: It is possible for a signed pointer to have a signature
        // that is all zeroes. This is indistinguishable from a raw pointer.
        // This code will treat such a pointer as signed and re-sign it. A
        // false positive is safe: method list pointers are either authed or
        // stripped, so if baseMethods() doesn't expect it to be signed, it
        // will ignore the signature.
        void *strippedBaseMethodList = ptrauth_strip(baseMethodList, ptrauth_key_method_list_pointer);
        void *signedBaseMethodList = ptrauth_sign_unauthenticated(strippedBaseMethodList,
                                                                  ptrauth_key_method_list_pointer,
                                                                  baseMethodListPtrauthData());
        if (baseMethodList == signedBaseMethodList) {
            ro->baseMethodList = ptrauth_auth_and_resign(baseMethodList,
                                                         ptrauth_key_method_list_pointer,
                                                         baseMethodListPtrauthData(),
                                                         ptrauth_key_method_list_pointer,
                                                         ro->baseMethodListPtrauthData());
        } else {
            // Special case: a class_ro_t in the shared cache pointing to a
            // method list in the shared cache will not have a signed pointer,
            // but the duplicate will be expected to have a signed pointer since
            // it's not in the shared cache. Detect that and sign it.
            bool roInSharedCache = objc::inSharedCache((uintptr_t)this);
            bool listInSharedCache = objc::inSharedCache((uintptr_t)strippedBaseMethodList);
            if (roInSharedCache && listInSharedCache)
                ro->baseMethodList = ptrauth_sign_unauthenticated(strippedBaseMethodList,
                                                                  ptrauth_key_method_list_pointer,
                                                                  ro->baseMethodListPtrauthData());
        }
#endif

        return ro;
    }
    
    // class is a metaclass
    //#define RO_META               (1<<0)
    Class getNonMetaclass() const {
        ASSERT(flags & RO_META);
        return nonMetaclass;
    }

    const uint8_t *getIvarLayout() const {
        if (flags & RO_META)
            return nullptr;
        return ivarLayout;
    }
};


/***********************************************************************
* list_array_tt<Element, List, Ptr>
* Generic implementation for metadata that can be augmented by categories.
 可以按类别扩展的元数据的通用实现。
*
* 实际应用：
* 1. class method_array_t : list_array_tt<method_t, method_list_t, method_list_t_authed_ptr>
* 2. class property_array_t : public list_array_tt<property_t, property_list_t, RawPtr>
* 3. class protocol_array_t : public list_array_tt<protocol_ref_t, protocol_list_t, RawPtr>
*
*
* Element is the underlying metadata type (e.g. method_t)
 Element 是基础元数据类型（例如: method_t）
* List is the metadata's list type (e.g. method_list_t)
 List 是元数据的列表类型（例如: method_list_t）
* List is a template applied to Element to make Element*. Useful for
* applying qualifiers to the pointer type.
*
* A list_array_tt has one of three values:
 list_array_tt 具有如下三个值之一：
* - empty （空）
* - a pointer to a single list （指向单个列表的指针）(array_t *)
* - an array of pointers to lists （指向列表的指针数组）(List* lists[0] 元素是 List * 的数组)
*
* countLists/beginLists/endLists iterate the metadata lists
 countLists/beginLists/endLists 迭代元数据列表。
* count/begin/end iterate the underlying metadata elements
 count/begin/end 迭代基础元数据元素。
**********************************************************************/
template <typename Element, typename List, template<typename> class Ptr>
class list_array_tt {
    struct array_t {
        // count 是 lists 数组中 List * 的数量
        uint32_t count;
        // 数组中的元素是 List * (实际类型是 entsize_list_tt *的WrappedPtr包装)
        Ptr<List> lists[0];

        // 所占用的字节总数
        static size_t byteSize(uint32_t count) {
            return sizeof(array_t) + count*sizeof(lists[0]);
        }
        
        // 根据 count 计算所有的字节总数
        size_t byteSize() {
            return byteSize(count);
        }
    };

    //protected 部分则是自定义的迭代器。
 protected:
    class iterator {
        // 指向指针的指针，且中间夹了一个 const 修饰，
        // const 表示前半部分 List * 不可更改。
        // lists 是一个指向指针的指针，表示它指向的这个指针的指向不可更改。
        
        // 对应 array_t 中的 List* lists[0]
        const Ptr<List> *lists;
        const Ptr<List> *listsEnd;
        
        // 这里的 List 都是 entsize_list_tt 类型即 m 和 mEnd 是 entsize_list_tt::iterator 类型
        typename List::iterator m, mEnd;

     public:
        // 构造函数，初始化列表初始化 lists、listsEnd，
        // if 内部，*begin 是 entsize_list_tt 指针
        iterator(const Ptr<List> *begin, const Ptr<List> *end)
            : lists(begin), listsEnd(end)
        {
            if (begin != end) {
                // m 和 mEnd 分别是指向 List* lists[0] 数组中 *begin 列表的第一个元素和最后一个元素的迭代器
                m = (*begin)->begin();
                mEnd = (*begin)->end();
            }
        }

        // 重载 * 操作符
        const Element& operator * () const {
            return *m;
        }
        
        // 重载 * 操作符
        Element& operator * () {
            return *m;
        }

        // 重载 != 操作符
        bool operator != (const iterator& rhs) const {
            if (lists != rhs.lists) return true;
            if (lists == listsEnd) return false;  // m is undefined
            if (m != rhs.m) return true;
            return false;
        }

        // 自增操作
        const iterator& operator ++ () {
            ASSERT(m != mEnd);
            
            // 这里是指迭代器指向的当前的方法列表的迭代器
            // (array_t 的 lists 中包含多个方法列表，每个列表迭代到 mEnd 后，会切换到下一个列表，并更新 m 和 mEnd)
            //
            // entsize_list_tt::iterator 自增
            m++;
            if (m == mEnd) {
                // 如果当前已经迭代到当前方法列表的末尾
                ASSERT(lists != listsEnd);
                
                // 自增，切到 array_t 的 lists 数组中的下一个方法列表中
                lists++;
                if (lists != listsEnd) {
                    // 更新新的方法列表的 m 和 mEnd
                    m = (*lists)->begin();
                    mEnd = (*lists)->end();
                }
            }
            
            // 取出 iterator 的内容
            return *this;
        }
    };

    // 刚刚两块都是定义在 list_array_tt 中的独立部分 struct array_t 和 class iterator。
    // 下面开始是 class list_array_tt 自己的内容
 private:
    
    // 联合体，包含两种情况：
    union {
        // list_array_tt 存储一个 entsize_list_tt 指针，保存一组内容（如只有一组 method_t）。
        Ptr<List> list;
        
        // list_array_tt 存储一个 array_t 指针，array_t 中是 entsize_list_tt * lists[0]，
        // 存储 entsize_list_tt * 的数组。
        //（如多组 method_t。如给某个类编写了多个 category，每个 category 的实例方法数组会被一组一组追加进来，
        // 而不是说不同 category 的实例方法统一追加到一个大数组中）
        uintptr_t arrayAndFlag;
    };

    // 第 1 位标识是指向单个列表的指针还是指向列表的指针数组。
    // 如果是 1 表示是指向列表的指针数组，即使用 array_t。
    bool hasArray() const {
        return arrayAndFlag & 1;
    }

    // arrayAndFlag 第 1 位置为 0，其它位保持不变，然后强转为 array_t *
    // （第 1 位只是用做标识位，真正使用 arrayAndFlag 的值时需要把第 1 位置回 0）
    array_t *array() const {
        return (array_t *)(arrayAndFlag & ~1);
    }

    // 把 array_t *array 强转为 uintptr_t，
    // 并把第 1 位置为 0，标识当前 list_array_t 内部数据使用的是 array_t
    void setArray(array_t *array) {
        arrayAndFlag = (uintptr_t)array | 1;
    }

    void validate() {
        for (auto cursor = beginLists(), end = endLists(); cursor != end; cursor++)
            cursor->validate();
    }

    
    //list_array_tt public 部分
 public:
    // 构造函数
    list_array_tt() : list(nullptr) { }
    list_array_tt(List *l) : list(l) { }
    list_array_tt(const list_array_tt &other) {
        *this = other;
    }

    list_array_tt &operator =(const list_array_tt &other) {
        if (other.hasArray()) {
            arrayAndFlag = other.arrayAndFlag;
        } else {
            list = other.list;
        }
        return *this;
    }

    // 统计所有元素的个数，注意这里是所有 Element 的个数
    uint32_t count() const {
        uint32_t result = 0;
        for (auto lists = beginLists(), end = endLists(); 
             lists != end;
             ++lists)
        {
            // 例如使用 entsize_list_tt -> count，统计 lists 中每个方法列表中的 method_t 的数量
            result += (*lists)->count;
        }
        return result;
    }

    // begin 迭代器
    iterator begin() const {
        return iterator(beginLists(), endLists());
    }

    // end 迭代器
    iterator end() const {
        auto e = endLists();
        return iterator(e, e);
    }

    // 例如方法列表数量（属性列表数量，协议列表数量等）（例如: method_array_t 下方法列表的数量）
    inline uint32_t countLists(const std::function<const array_t * (const array_t *)> & peek) const {
        if (hasArray()) {
            // 如果此时是 array_t，则返回其 count
            return peek(array())->count;
        } else if (list) {
            // 如果此时是指向单个列表的指针，则仅有 1 个方法列表
            return 1;
        } else {
            // 其它为 0
            return 0;
        }
    }

    uint32_t countLists() {
        return countLists([](array_t *x) { return x; });
    }

    // Lists 的起始地址
    const Ptr<List>* beginLists() const {
        if (hasArray()) {
            // 如果此时是指向列表的指针数组，则直接返回 lists
            return array()->lists;
        } else {
            // 如果此时是指向单个列表的指针，则 & 取出其地址返回
            return &list;
        }
    }

    // Lists 的 end 位置（注意 end 位置是指最后一个元素的后面，不是指最后一个元素）
    const Ptr<List>* endLists() const {
        if (hasArray()) {
            // 指针偏移
            return array()->lists + array()->count;
        } else if (list) {
            // &list + 1 偏移到整个指针的后面
            return &list + 1;
        } else {
            // 为空时，如果 list 为空，那对空取地址应该是 0x0
            return &list;
        }
    }

    // 附加 Lists，这里分三种情况
    void attachLists(List* const * addedLists, uint32_t addedCount) {
        // 如果 addedCount 为 0，则直接返回
        if (addedCount == 0) return;

        // 1): 如果目前是指向列表的指针数组，即把 addedLists 追加到 List* lists[0] 数组中
        if (hasArray()) {
            // many lists -> many lists
            uint32_t oldCount = array()->count;
            uint32_t newCount = oldCount + addedCount;
            // 内部 realloc 函数申请空间，同时 setArray 函数把第 1 位置为 1，作为标记
            array_t *newArray = (array_t *)malloc(array_t::byteSize(newCount));
            
            newArray->count = newCount;
            // 更新 count 值
            array()->count = newCount;

            // 把原始的 array()->lists 移动到了后面的内存空间中，
            // 前面空出了 [array()->lists, array()->lists + addedCount] 的空间
            for (int i = oldCount - 1; i >= 0; i--)
                newArray->lists[i + addedCount] = array()->lists[i];
            
            // 把要新追加的 addedLists 添加到上面预留出的空间
            //（这里证明了分类中添加的同名函数会 "覆盖" 类定义中的原始函数）
            for (unsigned i = 0; i < addedCount; i++)
                newArray->lists[i] = addedLists[i];
            free(array());
            setArray(newArray);
            validate();
        }
        
        // 2): 如果目前 lists 不存在并且 addedCount 等于 1，则直接把 addedLists 赋值给 list
        else if (!list  &&  addedCount == 1) {
            // 0 lists -> 1 list
            // 此时只保存一个方法列表
            list = addedLists[0];
            validate();
        }
        
        // 3): 如果目前是指向单个列表的指针需要转化为指向列表的指针数组
        else {
            // 1 list -> many lists
            Ptr<List> oldList = list;
            uint32_t oldCount = oldList ? 1 : 0;
            uint32_t newCount = oldCount + addedCount;
            
            // 内部 realloc 函数申请空间，setArray 函数把第 1 位置为 1，作为标记
            // 因为是新开辟空间，所以用的 malloc
            setArray((array_t *)malloc(array_t::byteSize(newCount)));
            
            // 更新 count
            array()->count = newCount;
            
            // 这里同样也是把 oldList 放在后面
            if (oldList) array()->lists[addedCount] = oldList;
            for (unsigned i = 0; i < addedCount; i++)
                array()->lists[i] = addedLists[i];
            validate();
        }
    }

    
//    static void try_free(const void *p)
//    {
//        if (p && malloc_size(p)) free((void *)p);
//    }
    // 如果 p 不为空且系统为其开辟了空间则执行 free 函数

    // 释放内存空间
    void tryFree() {
        if (hasArray()) {
            // 如果当前是指向列表的指针数组，首先进行循环释放列表
            for (uint32_t i = 0; i < array()->count; i++) {
                try_free(array()->lists[i]);
            }
            
            // 最后释放 array()
            try_free(array());
        }
        else if (list) {
            // 如果当前仅有一个方法列表
            
            //释放list
            try_free(list);
        }
    }

    // 复制一份 list_array_t
    template<typename Other>
    void duplicateInto(Other &other) {
        if (hasArray()) {
            array_t *a = array();
            other.setArray((array_t *)memdup(a, a->byteSize()));
            for (uint32_t i = 0; i < a->count; i++) {
                other.array()->lists[i] = a->lists[i]->duplicate();
            }
        } else if (list) {
            other.list = list->duplicate();
        } else {
            other.list = nil;
        }
    }
};


DECLARE_AUTHED_PTR_TEMPLATE(method_list_t)
//template <typename T> using method_list_t_authed_ptr = RawPtr<T>;
class method_array_t : 
    public list_array_tt<method_t, method_list_t, method_list_t_authed_ptr>
{
    // 类型定义
    typedef list_array_tt<method_t, method_list_t, method_list_t_authed_ptr> Super;

 public:
    method_array_t() : Super() { }
    method_array_t(method_list_t *l) : Super(l) { }

    // category 添加的函数的起始地址，由于 category 函数会追加到函数列表的最前面，
    // 所以 beginLists 就是 beginCategoryMethodLists
    const method_list_t_authed_ptr<method_list_t> *beginCategoryMethodLists() const {
        return beginLists();
    }
    
    // 分类添加的函数的结束地址，实现在 objc-runtime-new.mm 中
    const method_list_t_authed_ptr<method_list_t> *endCategoryMethodLists(Class cls) const;
};


class property_array_t : 
    public list_array_tt<property_t, property_list_t, RawPtr>
{
    // 类型声明
    typedef list_array_tt<property_t, property_list_t, RawPtr> Super;

 public:
    // 构造函数
    property_array_t() : Super() { }
    property_array_t(property_list_t *l) : Super(l) { }
};


class protocol_array_t : 
    public list_array_tt<protocol_ref_t, protocol_list_t, RawPtr>
{
    // 类型声明
    typedef list_array_tt<protocol_ref_t, protocol_list_t, RawPtr> Super;

 public:
    // 构造函数
    protocol_array_t() : Super() { }
    protocol_array_t(protocol_list_t *l) : Super(l) { }
};

struct class_rw_ext_t {
    DECLARE_AUTHED_PTR_TEMPLATE(class_ro_t)
    // 特别关注 ro 这个成员变量。
    // 这个即是在类实现完成后，class_rw_t 中存放的编译器生成的 class_ro_t 数据。
    class_ro_t_authed_ptr<const class_ro_t> ro;
    
    // 在上一节 class_ro_t 中的：
    // 方法列表、属性列表、成员变量列表、协议列表的类型如下:
    // struct method_list_t : entsize_list_tt<method_t, method_list_t, 0x3>
    // struct property_list_t : entsize_list_tt<property_t, property_list_t, 0>
    // struct ivar_list_t : entsize_list_tt<ivar_t, ivar_list_t, 0>
    // struct protocol_list_t
    
    // 到 class_rw_t 中就变为了:
    // class method_array_t : public list_array_tt<method_t, method_list_t>
    // class property_array_t : public list_array_tt<property_t, property_list_t>
    // class protocol_array_t : public list_array_tt<protocol_ref_t, protocol_list_t>
    
    // 这里先不着急，等下会详细分析它们所使用的新的数据结构: list_array_tt。
    
    // 方法列表
    method_array_t methods;
    // 属性列表
    property_array_t properties;
    // 协议列表
    protocol_array_t protocols;
    // 所属的类名
    char *demangledName;
    // 版本号
    uint32_t version;
};

struct class_rw_t {
    // Be warned that Symbolication knows the layout of this structure.
    // 请注意，Symbolication 知道此结构的布局。
    
    // 测试代码中：flags 打印看到是 2148007936
    // 转为二进制的话是只有 31 位和 19 位是 1，其它位全部都是 0，对应于:
    // class has started realizing but not yet completed it
    // class_data_bits_t->data() is class_ro_t
    // #define RW_REALIZING (1<<19)
    // class_data_bits_t->data() is class_rw_t, not class_ro_t
    // #define RW_REALIZED (1<<31)
    uint32_t flags;
    
    //（控制台打印值为 1）
    uint16_t witness;
#if SUPPORT_INDEXED_ISA
    // isa 中保存 indexcls，大概是 watchOS 下才会用到
    uint16_t index;
#endif

    // std::atomic<uintptr_t>
    // 原子性 unsigned long
    
    // 执行如下命令，会打印 error:
    // (lldb) p $2->ro_or_rw_ext
    // error: no member named 'ro_or_rw_ext' in 'class_rw_t'
    
    // 在编译时会根据类定义生成类的 class_ro_t 数据，其中包含方法列表、属性列表、成员变量列表等等内容
    
    // ro_or_rw_ext 会有两种情况：
    // 1): 编译时值是 class_ro_t *
    // 2): 编译后类实现完成后值是 class_rw_ext_t *，而编译时的 class_ro_t * 作为 class_rw_ext_t 的 const class_ro_t *ro 成员变量保存
    // 变量名字对应与 class_ro_t or(或) class_rw_ext_t
    explicit_atomic<uintptr_t> ro_or_rw_ext;

    // 当前所属类的第一个子类
    // 测试时，定义了一个继承自 NSObject 的类，
    // 控制台打印看到它的 firstSubclass 是 nil
    Class firstSubclass;
    
    // 哥哥类
    // 是继成同一个父类的类
    // ZStudent和ZTeacher都继承自ZPerson，
    // 但是ZStudent比ZTeacher先定义，ZStudent的nextSiblingClass是nil, ZTeacher的nextSiblingClass是ZStudent
    // 所以理解成哥哥类
    
    // firstSubclass 和 nextSiblingClass 有超级重要的作用，后面会展开
    Class nextSiblingClass;

private:
    // 使用 using 关键字声明一个 ro_or_rw_ext_t 类型:
    // objc::PointerUnion<const class_ro_t *, class_rw_ext_t *>
    //（可理解为一个指针联合体，系统只为其分配一个指针的内存空间，
    // 一次只能保存 class_ro_t 指针或者 class_rw_ext_t 指针）
    
    // 此时会发现 class_rw_t 一些端倪了。
    // 在 class_ro_t 中它是直接定义不同的成员变量来保存数据，
    // 而在 class_rw_t 中，它大概是用了一个中间人 struct class_rw_ext_t 来保存相关的数据。
    
    // 这里的数据存储根据类是否已经完成实现而分为两种情况：
    // 1): 类未实现完成时，ro_or_rw_ext 中存储的是 class_ro_t *
    // 2): 类已完成实现时，ro_or_rw_ext 中存储的是 class_rw_ext_t *，
    //     而 class_ro_t * 存储在 class_rw_ext_t 的 const class_ro_t *ro 成员变量中。
    
    // 类的 class_ro_t 类型的数据是在编译时就产生了。🌿

    using ro_or_rw_ext_t = objc::PointerUnion<const class_ro_t, class_rw_ext_t, PTRAUTH_STR("class_ro_t"), PTRAUTH_STR("class_rw_ext_t")>;

    // 根据 ro_or_rw_ext 获得 ro_or_rw_ext_t 类型的值。
    //（可能是 class_ro_t * 或者 class_rw_ext_t *）
    const ro_or_rw_ext_t get_ro_or_rwe() const {
        return ro_or_rw_ext_t{ro_or_rw_ext};
    }

    // 以原子方式把入参 const class_ro_t *ro 保存到 ro_or_rw_ext 中
    void set_ro_or_rwe(const class_ro_t *ro) {
        ro_or_rw_ext_t{ro, &ro_or_rw_ext}.storeAt(ro_or_rw_ext, memory_order_relaxed);
    }

    // 先把入参 const class_ro_t *ro 赋值给入参 class_rw_ext_t *rwe 的 const class_ro_t *ro，
    // 然后以原子方式把入参 class_rw_ext_t *rwe 保存到 ro_or_rw_ext 中
    void set_ro_or_rwe(class_rw_ext_t *rwe, const class_ro_t *ro) {
        // the release barrier is so that the class_rw_ext_t::ro initialization
        // is visible to lockless readers
        
        // 赋值
        rwe->ro = ro;
        // 原子方式保存
        ro_or_rw_ext_t{rwe, &ro_or_rw_ext}.storeAt(ro_or_rw_ext, memory_order_release);
    }

    // 此处仅声明 extAlloc 函数
    //（此函数的功能是进行 class_rw_ext_t 的初始化）
    // extAlloc 定义位于 objc-runtime-new.mm 中，主要完成 class_rw_ext_t 变量的创建，以及把其保存在 class_rw_t 的 ro_or_rw_ext 中。
    class_rw_ext_t *extAlloc(const class_ro_t *ro, bool deep = false);

public:
    // 以原子方式进行或操作设置 flags 指定位为 1。
    // 通过或操作把 set 中值是 1 的位同样设置到 flags 中，
    // 同时 flags 中值为 1 的位会保持原值。
    void setFlags(uint32_t set)
    {
        __c11_atomic_fetch_or((_Atomic(uint32_t) *)&flags, set, __ATOMIC_RELAXED);
    }

    // 以原子方式进行与操作设置 flags 指定位为 0。
    // 通过与操作把 ~clear 中值是 0 的位同样设置到 flags 中，
    // 同时 flags 中的其它位保持原值。
    void clearFlags(uint32_t clear) 
    {
        __c11_atomic_fetch_and((_Atomic(uint32_t) *)&flags, ~clear, __ATOMIC_RELAXED);
    }

    // set and clear must not overlap
    // 设置和清除不得重叠
    // 首先把 set 位置为 1，然后再进行 ~clear
    void changeFlags(uint32_t set, uint32_t clear) 
    {
        ASSERT((set & clear) == 0);

        uint32_t oldf, newf;
        do {
            oldf = flags;
            newf = (oldf | set) & ~clear;
            
            // bool OSAtomicCompareAndSwap32Barrier(int32_t __oldValue,
            //                                      int32_t __newValue,
            //                                      volatile int32_t *__theValue );
            // 比较 __oldValue 是否与 __theValue 指针指向的内存位置的值匹配，如果匹配，
            // 则将 __newValue 的值存储到 __theValue 指向的内存位置，
            // 同时匹配时返回 true，否则返回 false。
            
            // 此 do while 循环只为保证 newf 的值正确保存到 flags 中。

        } while (!OSAtomicCompareAndSwap32Barrier(oldf, newf, (volatile int32_t *)&flags));
    }

    // 从 ro_or_rw_ext 中取得 class_rw_ext_t 指针
    class_rw_ext_t *ext() const {
        return get_ro_or_rwe().dyn_cast<class_rw_ext_t *>(&ro_or_rw_ext);
    }

    // 由 class_ro_t 构建一个 class_rw_ext_t，
    // 如果目前 ro_or_rw_ext 已经是 class_rw_ext_t 指针了，则直接返回
    // 如果目前 ro_or_rw_ext 是 class_ro_t 指针的话，
    // 根据 class_ro_t 的值构建 class_rw_ext_t 并把它的地址赋值给 class_rw_t 的 ro_or_rw_ext，
    // 且最后返回 class_rw_ext_t 指针。
    class_rw_ext_t *extAllocIfNeeded() {
        auto v = get_ro_or_rwe();
        if (fastpath(v.is<class_rw_ext_t *>())) {
            return v.get<class_rw_ext_t *>(&ro_or_rw_ext);
        } else {
            return extAlloc(v.get<const class_ro_t *>(&ro_or_rw_ext));
        }
    }

    // extAlloc 中 deepCopy 传递 true，ro 的方法列表会执行复制操作，
    // method_list_t *list = ro->baseMethods();
    // if (deepCopy) list = list->duplicate()
    // 且 rw 的 methods 中追加的是新复制的这份方法列表。
    class_rw_ext_t *deepCopy(const class_ro_t *ro) {
        return extAlloc(ro, true);
    }

    // 从 ro_or_rw_ext 中取得 class_ro_t 指针，
    // 如果此时 ro_or_rw_ext 中存放的是 class_rw_ext_t 指针，
    // 则返回 class_rw_ext_t 中的 const class_ro_t *ro
    const class_ro_t *ro() const {
        // 取得一个指针
        auto v = get_ro_or_rwe();
        if (slowpath(v.is<class_rw_ext_t *>())) {
            // 如果此时是 class_rw_ext_t 指针，则返回它的 ro
            return v.get<class_rw_ext_t *>(&ro_or_rw_ext)->ro;
        }
        
        // 如果此时正是 class_ro_t，则直接返回
        return v.get<const class_ro_t *>(&ro_or_rw_ext);
    }

    // 设置 ro，如果当前 ro_or_rw_ext 中保存的是 class_rw_ext_t 指针，
    // 则把 ro 赋值给 class_rw_ext_t 的 const class_ro_t *ro。
    
    // 如果此时 ro_or_rw_ext 中保存的是 class_ro_t *ro 的话，
    // 则以原子方式把入参 ro 保存到 ro_or_rw_ext 中。
    void set_ro(const class_ro_t *ro) {
        auto v = get_ro_or_rwe();
        if (v.is<class_rw_ext_t *>()) {
            v.get<class_rw_ext_t *>(&ro_or_rw_ext)->ro = ro;
        } else {
            set_ro_or_rwe(ro);
        }
    }

    // 方法列表获取
    // 1): class_rw_ext_t 的 method_array_t methods
    // 2): class_ro_t 的 method_list_t * baseMethodList 构建的 method_array_t
    const method_array_t methods() const {
        auto v = get_ro_or_rwe();
        if (v.is<class_rw_ext_t *>()) {
            return v.get<class_rw_ext_t *>(&ro_or_rw_ext)->methods;
        } else {
            return method_array_t{v.get<const class_ro_t *>(&ro_or_rw_ext)->baseMethods()};
        }
    }

    // 属性列表获取
    const property_array_t properties() const {
        auto v = get_ro_or_rwe();
        if (v.is<class_rw_ext_t *>()) {
            return v.get<class_rw_ext_t *>(&ro_or_rw_ext)->properties;
        } else {
            return property_array_t{v.get<const class_ro_t *>(&ro_or_rw_ext)->baseProperties};
        }
    }

    // 协议列表获取
    const protocol_array_t protocols() const {
        auto v = get_ro_or_rwe();
        if (v.is<class_rw_ext_t *>()) {
            return v.get<class_rw_ext_t *>(&ro_or_rw_ext)->protocols;
        } else {
            return protocol_array_t{v.get<const class_ro_t *>(&ro_or_rw_ext)->baseProtocols};
        }
    }
};


struct class_data_bits_t {
    //声明了 objc_class 为其友元类，objc_class 可以完全访问和调用 class_data_bits_t 的私有成员变量和私有方法。
    friend objc_class;

    // Values are the FAST_ flags above.
    //仅有的一个成员变量 uintptr_t bits，这里之所以把它命名为 bits 也是有其意义的，它通过掩码的形式保存 class_rw_t 指针和是否是 swift 类等一些标志位。
    uintptr_t bits;
private:
    
    // 尾部的 const 表示该方法内部不会修改 class_data_bits_t 的内部数据
    // 这里返回值是一个 bool 类型，通过与操作来取出 bits 的指定位的值来进行判断
    bool getBit(uintptr_t bit) const
    {
        // 内部实现只有一个与操作，主要根据入参 bit(掩码) 来取得一些标识位
        // 如:
        
        // class is a Swift class from the pre-stable Swift ABI
        // #define FAST_IS_SWIFT_LEGACY    (1UL<<0)
        // 使用 bit 的第 0 位来进行判断
        
        // class is a Swift class from the stable Swift ABI
        // bit 的 第 1 位 判断类是否是稳定的 Swift ABI 的 Swift 类
        // #define FAST_IS_SWIFT_STABLE (1UL<<1)
        return bits & bit;
    }

    // Atomically set the bits in `set` and clear the bits in `clear`.
    // set and clear must not overlap.
    // 以原子方式设置 `set` 中的位，并清除 `clear` 中的位。设置和清除不得重叠。
    void setAndClearBits(uintptr_t set, uintptr_t clear)
    {
        // 断言，如果 set 和 clear 都是 1 的则没有执行的必要，直接执行断言
        ASSERT((set & clear) == 0);
        
        // 临时变量保存旧值
        uintptr_t newBits, oldBits = LoadExclusive(&bits);
        
        // 当 &bits 与 oldBits 相同时，把 newBits 复制到 &bits，
        // 并返回 true，由于前面的 ! 取反，此时会结束 do while 循环
        
        // 当 &bits 与 oldBits 不同时，把 oldBits 复制到 &bits，
        // 并返回 false，由于前面的 ! 取反，此时会继续 do while 循环

        do {
            
            // 先拿 oldBits 和 set 做或操作保证所有 64 位的 1 都保留下来
            // 然后 ~clear 的操作已经把需要 clear 的位置为 0，然后无关的其他位都是 1
            // 最后和 ~clear 做与操作并把结果赋值给 newBits，
            // 此时的 newBits 和原始的 bits 比的话，正是把入参 set 位置为 1 把入参 clear 位置为 0
            newBits = (oldBits | set) & ~clear;
            
            // while 循环正保证了 newBits 能正确的设置到的 bits 中
        } while (slowpath(!StoreReleaseExclusive(&bits, &oldBits, newBits)));
    }

    // 以原子方式设置 bits 中指定的标识位
    void setBits(uintptr_t set) {
        __c11_atomic_fetch_or((_Atomic(uintptr_t) *)&bits, set, __ATOMIC_RELAXED);
    }

    // 以原子方式清理 bits 中指定的标识位
    void clearBits(uintptr_t clear) {
        __c11_atomic_fetch_and((_Atomic(uintptr_t) *)&bits, ~clear, __ATOMIC_RELAXED);
    }

public:

    //从 bits 中读出 class_rw_t 指针
    
    // __LP64__: #define FAST_DATA_MASK 0x00007ffffffffff8UL
    // !__LP64__: #define FAST_DATA_MASK 0xfffffffcUL
    class_rw_t* data() const {
        // 与操作取出 class_rw_t 指针
        return (class_rw_t *)(bits & FAST_DATA_MASK);
    }
    
    // 上面是读取 class_rw_t 指针，那这里就是设置啦
    void setData(class_rw_t *newData)
    {
        // 断言点明了应该何时调用 setData 来设置 class_rw_t 指针
        // 虽然这里有个 newData->flags 我们还没有遇到，但是不影响我们分析当前情况
        
        // class has started realizing but not yet completed it
        // RW_REALIZING 标识类已经开始实现但是还没有完成，即类正在实现过程中
        
        //（标识在第十九位）
        // #define RW_REALIZING (1<<19)
        
        // class is unresolved future class
        // 类是尚未解决的未来类
        
        //（标识在第三十位）
        // #define RW_FUTURE (1<<30)
        
        // data() 不存在或者类处于 RW_REALIZING 或是 RW_FUTURE，否则执行断言
        ASSERT(!data()  ||  (newData->flags & (RW_REALIZING | RW_FUTURE)));
        
        
        // Set during realization or construction only. No locking needed.
        // setData 仅在 实现或构造期间 设置。不需要加锁。
        // Use a store-release fence because there may be concurrent
        // readers of data and data's contents.
        // 使用了一个 store-release fence 因为可能同时存在 数据读取器 和 数据内容读取器。
        
        
        // 首先是 bits 和 ~FAST_DATA_MASK 做一个与操作，即把 bits 中与 class_rw_t 指针相关的掩码位全部置为 0 同时保存其它位的值
        // 然后和 newData 做一个或操作，即把 newData 的地址有效位放到 bits 的对应的掩码位中bits 的其它位则保持原值
        
        // 把结果赋值给 newBits

        uintptr_t newBits = (bits & ~FAST_DATA_MASK) | (uintptr_t)newData;
        
    //线程围栏
        atomic_thread_fence(memory_order_release);
        //
        bits = newBits;
    }

    // Get the class's ro data, even in the presence of concurrent realization.
    // 即使存在并发实现，也获取类的 ro 数据。
    // fixme this isn't really safe without a compiler barrier at least
    // and probably a memory barrier when realizeClass changes the data field
    // fixme 这至少在没有编译器障碍的情况下并不是真正安全的，而当实现 Class 更改数据字段时可能没有内存障碍
    
    // 这里有一个点，大概我们之前已经见过 class_rw_t 和 class_ro_t 两者，
    // 它们两个一个是可读可写的，一个是只读的，
    // class_ro_t 中的内容都来自于 class 的定义中，当类没有实现完成时 data 函数返回的是 class_ro_t，
    // 当类实现完成后，class_ro_t 会赋值给 class_rw_t 的 ro 成员变量，
    // 且此时 data 函数返回也变为了 class_rw_t 指针。

    const class_ro_t *safe_ro() const {
        // 取得 data，这里用了一个 maybe_rw 的临时变量名，
        // 因为此时分两种情况，如果类已经实现完成，则 data 函数返回的是 class_rw_t 指针，
        // 而如果类没有实现完成的话返回的是 class_ro_t 指针
        class_rw_t *maybe_rw = data();
        
        // #define RW_REALIZED (1<<31)
        // class_rw_t->flags 的第 31 位标识了类是否已经实现完成
        if (maybe_rw->flags & RW_REALIZED) {
            // maybe_rw is rw
            
            // 如果类已经实现完成的话把 maybe_rw->ro() 返回，
            // 正是 class_ro_t 指针
            return maybe_rw->ro();
        } else {
            // maybe_rw is actually ro
            
            // 如果类是未实现完成状态的话，此时 data 函数返回的就是 class_ro_t *
            return (class_ro_t *)maybe_rw;
        }
    }

#if SUPPORT_INDEXED_ISA
    // 设置当前类在类的全局列表中的索引，此函数只针对于 isa 中是保存类索引的情况（目前的话大概是适用于 watchOS）
    void setClassArrayIndex(unsigned Idx) {
        // 0 is unused as then we can rely on zero-initialisation from calloc.
        // 0 未使用，因为我们可以依靠 calloc 的零初始化。
        ASSERT(Idx > 0);
        
        // 设置索引
        data()->index = Idx;
    }
#else
    void setClassArrayIndex(__unused unsigned Idx) {
    }
#endif

    // 获取类索引，同样仅适用于 isa 中保存 indexcls 的情况
    unsigned classArrayIndex() {
#if SUPPORT_INDEXED_ISA
        return data()->index;
#else
        return 0;
#endif
    }

    
    // 下面是一组和 Swift 相关的内容，都是以掩码的形式设置标识位或者读取标识位
    //是否是swift 类
    bool isAnySwift() {
        return isSwiftStable() || isSwiftLegacy();
    }

    // 类是否是稳定的 Swift ABI 的 Swift 类
    bool isSwiftStable() {
        // #define FAST_IS_SWIFT_STABLE (1UL<<1)
        // FAST_IS_SWIFT_STABLE 和 bits 做与操作，
        // 即取出 bits 的第二位标识位的值
        return getBit(FAST_IS_SWIFT_STABLE);
    }
    
    // 设置 FAST_IS_SWIFT_STABLE 同时清理 FAST_IS_SWIFT_LEGACY
    void setIsSwiftStable() {
        setAndClearBits(FAST_IS_SWIFT_STABLE, FAST_IS_SWIFT_LEGACY);
    }

    // 类是否是自稳定的 Swift ABI 的 Swift 类
    bool isSwiftLegacy() {
        // #define FAST_IS_SWIFT_LEGACY (1UL<<0)
          // FAST_IS_SWIFT_LEGACY 和 bits 做与操作，
          // 即取出 bits 的第一位标识位的值
        return getBit(FAST_IS_SWIFT_LEGACY);
    }
    
    // 设置 FAST_IS_SWIFT_LEGACY 同时清理 FAST_IS_SWIFT_STABLE
    void setIsSwiftLegacy() {
        setAndClearBits(FAST_IS_SWIFT_LEGACY, FAST_IS_SWIFT_STABLE);
    }

    // fixme remove this once the Swift runtime uses the stable bits
    // fixme 一旦 Swift 运行时使用稳定位将其删除
    bool isSwiftStable_ButAllowLegacyForNow() {
        return isAnySwift();
    }

    // 当分析 class_ro_t 时，我们再详细分析此函数
    _objc_swiftMetadataInitializer swiftMetadataInitializer() {
      
        // This function is called on un-realized
        // classes without holding any locks.
        // 在未实现的类上调用此函数，无需持有任何锁。
        
        // Beware of races with other realizers.
        // 当心与其它 realizers 的竞态。
        
        return safe_ro()->swiftMetadataInitializer();
    }
};


struct objc_class : objc_object {
  objc_class(const objc_class&) = delete;
  objc_class(objc_class&&) = delete;
  void operator=(const objc_class&) = delete;
  void operator=(objc_class&&) = delete;
    // Class ISA;     //8字节
    Class superclass; //8字节
    cache_t cache;    //16字节         // formerly cache pointer and vtable
    class_data_bits_t bits;    // class_rw_t * plus custom rr/alloc flags

    Class getSuperclass() const {
#if __has_feature(ptrauth_calls)
#   if ISA_SIGNING_AUTH_MODE == ISA_SIGNING_AUTH
        if (superclass == Nil)
            return Nil;

#if SUPERCLASS_SIGNING_TREAT_UNSIGNED_AS_NIL
        void *stripped = ptrauth_strip((void *)superclass, ISA_SIGNING_KEY);
        if ((void *)superclass == stripped) {
            void *resigned = ptrauth_sign_unauthenticated(stripped, ISA_SIGNING_KEY, ptrauth_blend_discriminator(&superclass, ISA_SIGNING_DISCRIMINATOR_CLASS_SUPERCLASS));
            if ((void *)superclass != resigned)
                return Nil;
        }
#endif
            
        void *result = ptrauth_auth_data((void *)superclass, ISA_SIGNING_KEY, ptrauth_blend_discriminator(&superclass, ISA_SIGNING_DISCRIMINATOR_CLASS_SUPERCLASS));
        return (Class)result;

#   else
        return (Class)ptrauth_strip((void *)superclass, ISA_SIGNING_KEY);
#   endif
#else
        return superclass;
#endif
    }

    void setSuperclass(Class newSuperclass) {
#if ISA_SIGNING_SIGN_MODE == ISA_SIGNING_SIGN_ALL
        superclass = (Class)ptrauth_sign_unauthenticated((void *)newSuperclass, ISA_SIGNING_KEY, ptrauth_blend_discriminator(&superclass, ISA_SIGNING_DISCRIMINATOR_CLASS_SUPERCLASS));
#else
        superclass = newSuperclass;
#endif
    }

    class_rw_t *data() const {
        return bits.data();
    }
    void setData(class_rw_t *newData) {
        bits.setData(newData);
    }

    void setInfo(uint32_t set) {
        ASSERT(isFuture()  ||  isRealized());
        data()->setFlags(set);
    }

    void clearInfo(uint32_t clear) {
        ASSERT(isFuture()  ||  isRealized());
        data()->clearFlags(clear);
    }

    // set and clear must not overlap
    void changeInfo(uint32_t set, uint32_t clear) {
        ASSERT(isFuture()  ||  isRealized());
        ASSERT((set & clear) == 0);
        data()->changeFlags(set, clear);
    }

#if FAST_HAS_DEFAULT_RR
    bool hasCustomRR() const {
        return !bits.getBit(FAST_HAS_DEFAULT_RR);
    }
    void setHasDefaultRR() {
        bits.setBits(FAST_HAS_DEFAULT_RR);
    }
    void setHasCustomRR() {
        bits.clearBits(FAST_HAS_DEFAULT_RR);
    }
#else
    bool hasCustomRR() const {
        return !(bits.data()->flags & RW_HAS_DEFAULT_RR);
    }
    void setHasDefaultRR() {
        bits.data()->setFlags(RW_HAS_DEFAULT_RR);
    }
    void setHasCustomRR() {
        bits.data()->clearFlags(RW_HAS_DEFAULT_RR);
    }
#endif

#if FAST_CACHE_HAS_DEFAULT_AWZ
    bool hasCustomAWZ() const {
        return !cache.getBit(FAST_CACHE_HAS_DEFAULT_AWZ);
    }
    void setHasDefaultAWZ() {
        cache.setBit(FAST_CACHE_HAS_DEFAULT_AWZ);
    }
    void setHasCustomAWZ() {
        cache.clearBit(FAST_CACHE_HAS_DEFAULT_AWZ);
    }
#else
    bool hasCustomAWZ() const {
        return !(bits.data()->flags & RW_HAS_DEFAULT_AWZ);
    }
    void setHasDefaultAWZ() {
        bits.data()->setFlags(RW_HAS_DEFAULT_AWZ);
    }
    void setHasCustomAWZ() {
        bits.data()->clearFlags(RW_HAS_DEFAULT_AWZ);
    }
#endif

#if FAST_CACHE_HAS_DEFAULT_CORE
    bool hasCustomCore() const {
        return !cache.getBit(FAST_CACHE_HAS_DEFAULT_CORE);
    }
    void setHasDefaultCore() {
        return cache.setBit(FAST_CACHE_HAS_DEFAULT_CORE);
    }
    void setHasCustomCore() {
        return cache.clearBit(FAST_CACHE_HAS_DEFAULT_CORE);
    }
#else
    bool hasCustomCore() const {
        return !(bits.data()->flags & RW_HAS_DEFAULT_CORE);
    }
    void setHasDefaultCore() {
        bits.data()->setFlags(RW_HAS_DEFAULT_CORE);
    }
    void setHasCustomCore() {
        bits.data()->clearFlags(RW_HAS_DEFAULT_CORE);
    }
#endif

#if FAST_CACHE_HAS_CXX_CTOR
    bool hasCxxCtor() {
        ASSERT(isRealized());
        return cache.getBit(FAST_CACHE_HAS_CXX_CTOR);
    }
    void setHasCxxCtor() {
        cache.setBit(FAST_CACHE_HAS_CXX_CTOR);
    }
#else
    bool hasCxxCtor() {
        ASSERT(isRealized());
        return bits.data()->flags & RW_HAS_CXX_CTOR;
    }
    void setHasCxxCtor() {
        bits.data()->setFlags(RW_HAS_CXX_CTOR);
    }
#endif

#if FAST_CACHE_HAS_CXX_DTOR
    bool hasCxxDtor() {
        ASSERT(isRealized());
        return cache.getBit(FAST_CACHE_HAS_CXX_DTOR);
    }
    void setHasCxxDtor() {
        cache.setBit(FAST_CACHE_HAS_CXX_DTOR);
    }
#else
    bool hasCxxDtor() {
        ASSERT(isRealized());
        return bits.data()->flags & RW_HAS_CXX_DTOR;
    }
    void setHasCxxDtor() {
        bits.data()->setFlags(RW_HAS_CXX_DTOR);
    }
#endif

#if FAST_CACHE_REQUIRES_RAW_ISA
    bool instancesRequireRawIsa() {
        return cache.getBit(FAST_CACHE_REQUIRES_RAW_ISA);
    }
    void setInstancesRequireRawIsa() {
        cache.setBit(FAST_CACHE_REQUIRES_RAW_ISA);
    }
#elif SUPPORT_NONPOINTER_ISA
    bool instancesRequireRawIsa() {
        return bits.data()->flags & RW_REQUIRES_RAW_ISA;
    }
    void setInstancesRequireRawIsa() {
        bits.data()->setFlags(RW_REQUIRES_RAW_ISA);
    }
#else
    bool instancesRequireRawIsa() {
        return true;
    }
    void setInstancesRequireRawIsa() {
        // nothing
    }
#endif
    void setInstancesRequireRawIsaRecursively(bool inherited = false);
    void printInstancesRequireRawIsa(bool inherited);

#if CONFIG_USE_PREOPT_CACHES
    bool allowsPreoptCaches() const {
        return !(bits.data()->flags & RW_NOPREOPT_CACHE);
    }
    bool allowsPreoptInlinedSels() const {
        return !(bits.data()->flags & RW_NOPREOPT_SELS);
    }
    void setDisallowPreoptCaches() {
        bits.data()->setFlags(RW_NOPREOPT_CACHE | RW_NOPREOPT_SELS);
    }
    void setDisallowPreoptInlinedSels() {
        bits.data()->setFlags(RW_NOPREOPT_SELS);
    }
    void setDisallowPreoptCachesRecursively(const char *why);
    void setDisallowPreoptInlinedSelsRecursively(const char *why);
#else
    bool allowsPreoptCaches() const { return false; }
    bool allowsPreoptInlinedSels() const { return false; }
    void setDisallowPreoptCaches() { }
    void setDisallowPreoptInlinedSels() { }
    void setDisallowPreoptCachesRecursively(const char *why) { }
    void setDisallowPreoptInlinedSelsRecursively(const char *why) { }
#endif

    bool canAllocNonpointer() {
        ASSERT(!isFuture());
        return !instancesRequireRawIsa();
    }

    bool isSwiftStable() {
        return bits.isSwiftStable();
    }

    bool isSwiftLegacy() {
        return bits.isSwiftLegacy();
    }

    bool isAnySwift() {
        return bits.isAnySwift();
    }

    bool isSwiftStable_ButAllowLegacyForNow() {
        return bits.isSwiftStable_ButAllowLegacyForNow();
    }

    uint32_t swiftClassFlags() {
        return *(uint32_t *)(&bits + 1);
    }
  
    bool usesSwiftRefcounting() {
        if (!isSwiftStable()) return false;
        return bool(swiftClassFlags() & 2); //ClassFlags::UsesSwiftRefcounting
    }

    bool canCallSwiftRR() {
        // !hasCustomCore() is being used as a proxy for isInitialized(). All
        // classes with Swift refcounting are !hasCustomCore() (unless there are
        // category or swizzling shenanigans), but that bit is not set until a
        // class is initialized. Checking isInitialized requires an extra
        // indirection that we want to avoid on RR fast paths.
        //
        // In the unlikely event that someone causes a class with Swift
        // refcounting to be hasCustomCore(), we'll fall back to sending -retain
        // or -release, which is still correct.
        return !hasCustomCore() && usesSwiftRefcounting();
    }

    bool isStubClass() const {
        uintptr_t isa = (uintptr_t)isaBits();
        return 1 <= isa && isa < 16;
    }

    // Swift stable ABI built for old deployment targets looks weird.
    // The is-legacy bit is set for compatibility with old libobjc.
    // We are on a "new" deployment target so we need to rewrite that bit.
    // These stable-with-legacy-bit classes are distinguished from real
    // legacy classes using another bit in the Swift data
    // (ClassFlags::IsSwiftPreStableABI)

    bool isUnfixedBackwardDeployingStableSwift() {
        // Only classes marked as Swift legacy need apply.
        if (!bits.isSwiftLegacy()) return false;

        // Check the true legacy vs stable distinguisher.
        // The low bit of Swift's ClassFlags is SET for true legacy
        // and UNSET for stable pretending to be legacy.
        bool isActuallySwiftLegacy = bool(swiftClassFlags() & 1);
        return !isActuallySwiftLegacy;
    }

    void fixupBackwardDeployingStableSwift() {
        if (isUnfixedBackwardDeployingStableSwift()) {
            // Class really is stable Swift, pretending to be pre-stable.
            // Fix its lie.
            bits.setIsSwiftStable();
        }
    }

    _objc_swiftMetadataInitializer swiftMetadataInitializer() {
        return bits.swiftMetadataInitializer();
    }

    // Return YES if the class's ivars are managed by ARC, 
    // or the class is MRC but has ARC-style weak ivars.
    bool hasAutomaticIvars() {
        return data()->ro()->flags & (RO_IS_ARC | RO_HAS_WEAK_WITHOUT_ARC);
    }

    // Return YES if the class's ivars are managed by ARC.
    bool isARC() {
        return data()->ro()->flags & RO_IS_ARC;
    }


    bool forbidsAssociatedObjects() {
        return (data()->flags & RW_FORBIDS_ASSOCIATED_OBJECTS);
    }

#if SUPPORT_NONPOINTER_ISA
    // Tracked in non-pointer isas; not tracked otherwise
#else
    bool instancesHaveAssociatedObjects() {
        // this may be an unrealized future class in the CF-bridged case
        ASSERT(isFuture()  ||  isRealized());
        return data()->flags & RW_INSTANCES_HAVE_ASSOCIATED_OBJECTS;
    }

    void setInstancesHaveAssociatedObjects() {
        // this may be an unrealized future class in the CF-bridged case
        ASSERT(isFuture()  ||  isRealized());
        setInfo(RW_INSTANCES_HAVE_ASSOCIATED_OBJECTS);
    }
#endif

    bool shouldGrowCache() {
        return true;
    }

    void setShouldGrowCache(bool) {
        // fixme good or bad for memory use?
    }

    bool isInitializing() {
        return getMeta()->data()->flags & RW_INITIALIZING;
    }

    void setInitializing() {
        ASSERT(!isMetaClass());
        ISA()->setInfo(RW_INITIALIZING);
    }

    bool isInitialized() {
        return getMeta()->data()->flags & RW_INITIALIZED;
    }

    void setInitialized();

    bool isLoadable() {
        ASSERT(isRealized());
        return true;  // any class registered for +load is definitely loadable
    }

    IMP getLoadMethod();

    // Locking: To prevent concurrent realization, hold runtimeLock.
    bool isRealized() const {
        return !isStubClass() && (data()->flags & RW_REALIZED);
    }

    // Returns true if this is an unrealized future class.
    // Locking: To prevent concurrent realization, hold runtimeLock.
    bool isFuture() const {
        if (isStubClass())
            return false;
        return data()->flags & RW_FUTURE;
    }

    bool isMetaClass() const {
        ASSERT_THIS_NOT_NULL;
        ASSERT(isRealized());
#if FAST_CACHE_META
        return cache.getBit(FAST_CACHE_META);
#else
        return data()->flags & RW_META;
#endif
    }

    // Like isMetaClass, but also valid on un-realized classes
    bool isMetaClassMaybeUnrealized() {
        static_assert(offsetof(class_rw_t, flags) == offsetof(class_ro_t, flags), "flags alias");
        static_assert(RO_META == RW_META, "flags alias");
        if (isStubClass())
            return false;
        return data()->flags & RW_META;
    }

    // NOT identical to this->ISA when this is a metaclass
    Class getMeta() {
        if (isMetaClassMaybeUnrealized()) return (Class)this;
        else return this->ISA();
    }

    bool isRootClass() {
        return getSuperclass() == nil;
    }
    bool isRootMetaclass() {
        return ISA() == (Class)this;
    }
  
    // If this class does not have a name already, we can ask Swift to construct one for us.
    const char *installMangledNameForLazilyNamedClass();

    // Get the class's mangled name, or NULL if the class has a lazy
    // name that hasn't been created yet.
    const char *nonlazyMangledName() const {
        return bits.safe_ro()->getName();
    }

    const char *mangledName() { 
        // fixme can't assert locks here
        ASSERT_THIS_NOT_NULL;

        const char *result = nonlazyMangledName();

        if (!result) {
            // This class lazily instantiates its name. Emplace and
            // return it.
            result = installMangledNameForLazilyNamedClass();
        }

        return result;
    }
    
    const char *demangledName(bool needsLock);
    const char *nameForLogging();

    // May be unaligned depending on class's ivars.
    uint32_t unalignedInstanceStart() const {
        ASSERT(isRealized());
        return data()->ro()->instanceStart;
    }

    // Class's instance start rounded up to a pointer-size boundary.
    // This is used for ARC layout bitmaps.
    uint32_t alignedInstanceStart() const {
        return word_align(unalignedInstanceStart());
    }

    // May be unaligned depending on class's ivars.
    uint32_t unalignedInstanceSize() const {
        ASSERT(isRealized());
        return data()->ro()->instanceSize;
    }

    // Class's ivar size rounded up to a pointer-size boundary.
    uint32_t alignedInstanceSize() const {
        return word_align(unalignedInstanceSize());
    }

    inline size_t instanceSize(size_t extraBytes) const {
        if (fastpath(cache.hasFastInstanceSize(extraBytes))) {
            return cache.fastInstanceSize(extraBytes);
        }

        size_t size = alignedInstanceSize() + extraBytes;
        // CF requires all objects be at least 16 bytes.
        if (size < 16) size = 16;
        return size;
    }

    void setInstanceSize(uint32_t newSize) {
        ASSERT(isRealized());
        ASSERT(data()->flags & RW_REALIZING);
        auto ro = data()->ro();
        if (newSize != ro->instanceSize) {
            ASSERT(data()->flags & RW_COPIED_RO);
            *const_cast<uint32_t *>(&ro->instanceSize) = newSize;
        }
        cache.setFastInstanceSize(newSize);
    }

    void chooseClassArrayIndex();

    void setClassArrayIndex(unsigned Idx) {
        bits.setClassArrayIndex(Idx);
    }

    unsigned classArrayIndex() {
        return bits.classArrayIndex();
    }
};


struct swift_class_t : objc_class {
    uint32_t flags;
    uint32_t instanceAddressOffset;
    uint32_t instanceSize;
    uint16_t instanceAlignMask;
    uint16_t reserved;

    uint32_t classSize;
    uint32_t classAddressOffset;
    void *description;
    // ...

    void *baseAddress() {
        return (void *)((uint8_t *)this - classAddressOffset);
    }
};


struct category_t {
    const char *name;
    classref_t cls;
    WrappedPtr<method_list_t, PtrauthStrip> instanceMethods;
    WrappedPtr<method_list_t, PtrauthStrip> classMethods;
    struct protocol_list_t *protocols;
    struct property_list_t *instanceProperties;
    // Fields below this point are not always present on disk.
    struct property_list_t *_classProperties;

    method_list_t *methodsForMeta(bool isMeta) {
        if (isMeta) return classMethods;
        else return instanceMethods;
    }

    property_list_t *propertiesForMeta(bool isMeta, struct header_info *hi);
    
    protocol_list_t *protocolsForMeta(bool isMeta) {
        if (isMeta) return nullptr;
        else return protocols;
    }
};

struct objc_super2 {
    id receiver;
    Class current_class;
};

struct message_ref_t {
    IMP imp;
    SEL sel;
};


extern Method protocol_getMethod(protocol_t *p, SEL sel, bool isRequiredMethod, bool isInstanceMethod, bool recursive);

#endif
