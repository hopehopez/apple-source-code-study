/*
 * Copyright (c) 1999-2002, 2005-2008 Apple Inc.  All Rights Reserved.
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

#ifndef _OBJC_CONFIG_H_
#define _OBJC_CONFIG_H_

#include <TargetConditionals.h>

// Avoid the !NDEBUG double negative.
#if !NDEBUG
#   define DEBUG 1
#else
#   define DEBUG 0
#endif

// Define SUPPORT_GC_COMPAT=1 to enable compatibility where GC once was.
// OBJC_NO_GC and OBJC_NO_GC_API in objc-api.h mean something else.
#if !TARGET_OS_OSX
#   define SUPPORT_GC_COMPAT 0
#else
#   define SUPPORT_GC_COMPAT 1
#endif

// Define SUPPORT_ZONES=1 to enable malloc zone support in NXHashTable.
#if !(TARGET_OS_OSX || TARGET_OS_MACCATALYST)
#   define SUPPORT_ZONES 0
#else
#   define SUPPORT_ZONES 1
#endif

// Define SUPPORT_MOD=1 to use the mod operator in NXHashTable and objc-sel-set
#if defined(__arm__)
#   define SUPPORT_MOD 0
#else
#   define SUPPORT_MOD 1
#endif

// Define SUPPORT_PREOPT=1 to enable dyld shared cache optimizations
#if TARGET_OS_WIN32
#   define SUPPORT_PREOPT 0
#else
#   define SUPPORT_PREOPT 1
#endif

//定义 是否支持Tagged Pointer 非64位系统不支持
// Define SUPPORT_TAGGED_POINTERS=1 to enable tagged pointer objects
// Be sure to edit tagged pointer SPI in objc-internal.h as well.
#if !__LP64__
#   define SUPPORT_TAGGED_POINTERS 0
#else
#   define SUPPORT_TAGGED_POINTERS 1
#endif

// Define SUPPORT_MSB_TAGGED_POINTERS to use the MSB 
// as the tagged pointer marker instead of the LSB.
// Be sure to edit tagged pointer SPI in objc-internal.h as well.
#if !SUPPORT_TAGGED_POINTERS  ||  ((TARGET_OS_OSX || TARGET_OS_MACCATALYST) && __x86_64__)
#   define SUPPORT_MSB_TAGGED_POINTERS 0
#else
#   define SUPPORT_MSB_TAGGED_POINTERS 1
#endif

// Define SUPPORT_INDEXED_ISA=1 on platforms that store the class in the isa 
// field as an index into a class table.
// Note, keep this in sync with any .s files which also define it.
// Be sure to edit objc-abi.h as well.
#if __ARM_ARCH_7K__ >= 2  ||  (__arm64__ && !__LP64__)
#   define SUPPORT_INDEXED_ISA 1
#else
#   define SUPPORT_INDEXED_ISA 0
#endif

// Define SUPPORT_PACKED_ISA=1 on platforms that store the class in the isa 
// field as a maskable pointer with other data around it.
#if (!__LP64__  ||  TARGET_OS_WIN32  ||  \
     (TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST && !__arm64__))
#   define SUPPORT_PACKED_ISA 0
#else
#   define SUPPORT_PACKED_ISA 1
#endif

// Define SUPPORT_NONPOINTER_ISA=1 on any platform that may store something
// in the isa field that is not a raw pointer.
#if !SUPPORT_INDEXED_ISA  &&  !SUPPORT_PACKED_ISA
#   define SUPPORT_NONPOINTER_ISA 0
#else
#   define SUPPORT_NONPOINTER_ISA 1
#endif

// Define SUPPORT_FIXUP=1 to repair calls sites for fixup dispatch.
// Fixup messaging itself is no longer supported.
// Be sure to edit objc-abi.h as well (objc_msgSend*_fixup)
#if !(defined(__x86_64__) && (TARGET_OS_OSX || TARGET_OS_SIMULATOR))
#   define SUPPORT_FIXUP 0
#else
#   define SUPPORT_FIXUP 1
#endif

// Define SUPPORT_ZEROCOST_EXCEPTIONS to use "zero-cost" exceptions for OBJC2.
// Be sure to edit objc-exception.h as well (objc_add/removeExceptionHandler)
#if defined(__arm__)  &&  __USING_SJLJ_EXCEPTIONS__
#   define SUPPORT_ZEROCOST_EXCEPTIONS 0
#else
#   define SUPPORT_ZEROCOST_EXCEPTIONS 1
#endif

// Define SUPPORT_ALT_HANDLERS if you're using zero-cost exceptions 
// but also need to support AppKit's alt-handler scheme
// Be sure to edit objc-exception.h as well (objc_add/removeExceptionHandler)
#if !SUPPORT_ZEROCOST_EXCEPTIONS  ||  !TARGET_OS_OSX
#   define SUPPORT_ALT_HANDLERS 0
#else
#   define SUPPORT_ALT_HANDLERS 1
#endif

// Define SUPPORT_RETURN_AUTORELEASE to optimize autoreleased return values
#if TARGET_OS_WIN32
#   define SUPPORT_RETURN_AUTORELEASE 0
#else
#   define SUPPORT_RETURN_AUTORELEASE 1
#endif

// Define SUPPORT_STRET on architectures that need separate struct-return ABI.
#if defined(__arm64__)
#   define SUPPORT_STRET 0
#else
#   define SUPPORT_STRET 1
#endif

// Define SUPPORT_MESSAGE_LOGGING to enable NSObjCMessageLoggingEnabled
#if !TARGET_OS_OSX
#   define SUPPORT_MESSAGE_LOGGING 0
#else
#   define SUPPORT_MESSAGE_LOGGING 1
#endif

// Define SUPPORT_AUTORELEASEPOOL_DEDDUP_PTRS to combine consecutive pointers to the same object in autorelease pools
#if !__LP64__
#   define SUPPORT_AUTORELEASEPOOL_DEDUP_PTRS 0
#else
#   define SUPPORT_AUTORELEASEPOOL_DEDUP_PTRS 1
#endif

// Define HAVE_TASK_RESTARTABLE_RANGES to enable usage of
// task_restartable_ranges_synchronize()

// 定义 HAVE_TASK_RESTARTABLE_RANGES
// 以启用使用 task_restartable_ranges_synchronize() 函数

// 任务可 重新开始/可重新启动的 范围/区间

#if TARGET_OS_SIMULATOR || defined(__i386__) || defined(__arm__) || !TARGET_OS_MAC
#   define HAVE_TASK_RESTARTABLE_RANGES 0
#else
    // x86_64 和 arm64 平台下都是 1
#   define HAVE_TASK_RESTARTABLE_RANGES 1
#endif

// OBJC_INSTRUMENTED controls whether message dispatching is dynamically
// monitored.  Monitoring introduces substantial overhead.
// NOTE: To define this condition, do so in the build command, NOT by
// uncommenting the line here.  This is because objc-class.h heeds this
// condition, but objc-class.h can not #include this file (objc-config.h)
// because objc-class.h is public and objc-config.h is not.
// 监控会带来大量开销。
// NOTE: 若要定义此条件，请在 build 命令中执行此操作，而不是取消下面 OBJC_INSTRUMENTED 的注释。
// 这是因为 objc-class.h 注意到了这个条件，但是 objc-class.h 不能包括这个文件（objc-config.h），
// 因为 objc-class.h 是公共的，而 objc-config.h 不是。

// OBJC_INSTRUMENTED 控制是否动态监视消息调度。

//#define OBJC_INSTRUMENTED

// The runtimeLock is a mutex always held hence the cache lock is
// redundant and can be elided.
//
// If the runtime lock ever becomes a rwlock again,
// the cache lock would need to be used again
#define CONFIG_USE_CACHE_LOCK 0

// 三种方法缓存存储 IMP 的方式：（bucket_t 中 _imp 成员变量存储 IMP 的方式）
// Determine how the method cache stores IMPs.
// 确定方法缓存如何存储imp。 (IMP 是函数实现的指针，保存了函数实现的地址，根据 IMP 可以直接执行函数...)

// 方法缓存包含原始的 IMP（bucket_t 中 _imp 为 IMP）
#define CACHE_IMP_ENCODING_NONE 1 // Method cache contains raw IMP. 直接存储原始imp
// 方法缓存包含 ISA 与 IMP 的异或（bucket_t 中 _imp 是 IMP ^ ISA）
#define CACHE_IMP_ENCODING_ISA_XOR 2 // Method cache contains ISA ^ IMP. 存储ISA 和 IMP的异或值
// 方法缓存包含指针验证的 IMP (bucket_t 中 _imp 是 ptrauth_auth_and_resign 函数计算的值)
#define CACHE_IMP_ENCODING_PTRAUTH 3 // Method cache contains ptrauth'd IMP. 存储指针验证的imp ptrauth_auth_and_resign 函数计算的值

// 上述三种方式的各在什么平台使用：
#if __PTRAUTH_INTRINSICS__
// Always use ptrauth when it's supported. 当平台支持 __PTRAUTH_INTRINSICS__ 时总是使用指针验证
#define CACHE_IMP_ENCODING CACHE_IMP_ENCODING_PTRAUTH
#elif defined(__arm__)
// 32-bit ARM uses no encoding. 32-bit ARM uses no encoding. 32位 ARM 下不进行编码，直接使用原始 IMP(watchOS 下)
#define CACHE_IMP_ENCODING CACHE_IMP_ENCODING_NONE
#else
// Everything else uses ISA ^ IMP. 其它情况下在方法缓存中存储 ISA 与 IMP 异或的值
#define CACHE_IMP_ENCODING CACHE_IMP_ENCODING_ISA_XOR
#endif

// CACHE 中掩码位置
#define CACHE_MASK_STORAGE_OUTLINED 1               // 没有掩码
#define CACHE_MASK_STORAGE_HIGH_16 2                // 高 16 位
#define CACHE_MASK_STORAGE_LOW_4 3                  // 低 4 位
#define CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS 4      // 高16位 大端地址

#if defined(__arm64__) && __LP64__                  // 如果是 64 位 ARM 平台（iPhone 自 5s 起都是）
#if TARGET_OS_OSX || TARGET_OS_SIMULATOR            // 如果是mac os(ARM) 或者是iOS模拟器
#define CACHE_MASK_STORAGE CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS
#else                                               // 真机
#define CACHE_MASK_STORAGE CACHE_MASK_STORAGE_HIGH_16 // 掩码存储在高 16 位
#endif
#elif defined(__arm64__) && !__LP64__               //ARM 平台 非 64 位系统架构（watchOS 下）
#define CACHE_MASK_STORAGE CACHE_MASK_STORAGE_LOW_4 // 掩码存储在低 4 位
#else                                               // mac 系统
#define CACHE_MASK_STORAGE CACHE_MASK_STORAGE_OUTLINED // 不使用掩码的方式（_buckets 与 _mask 分别是两个变量，上面则是把 buckets 和 mask 合并保存在 _maskAndBuckets 中）

#endif

// Constants used for signing/authing isas. This doesn't quite belong
// here, but the asm files can't import other headers.
#define ISA_SIGNING_DISCRIMINATOR 0x6AE1
#define ISA_SIGNING_DISCRIMINATOR_CLASS_SUPERCLASS 0xB5AB

#define ISA_SIGNING_KEY ptrauth_key_process_independent_data

// ISA signing authentication modes. Set ISA_SIGNING_AUTH_MODE to one
// of these to choose how ISAs are authenticated.
#define ISA_SIGNING_STRIP 1 // Strip the signature whenever reading an ISA.
#define ISA_SIGNING_AUTH  2 // Authenticate the signature on all ISAs.


// ISA signing modes. Set ISA_SIGNING_SIGN_MODE to one of these to
// choose how ISAs are signed.
#define ISA_SIGNING_SIGN_NONE       1 // Sign no ISAs.
#define ISA_SIGNING_SIGN_ONLY_SWIFT 2 // Only sign ISAs of Swift objects.
#define ISA_SIGNING_SIGN_ALL        3 // Sign all ISAs.

#if __has_feature(ptrauth_objc_isa_strips) || __has_feature(ptrauth_objc_isa_signs) || __has_feature(ptrauth_objc_isa_authenticates)
#   if __has_feature(ptrauth_objc_isa_authenticates)
#       define ISA_SIGNING_AUTH_MODE ISA_SIGNING_AUTH
#   else
#       define ISA_SIGNING_AUTH_MODE ISA_SIGNING_STRIP
#   endif
#   if __has_feature(ptrauth_objc_isa_signs)
#       define ISA_SIGNING_SIGN_MODE ISA_SIGNING_SIGN_ALL
#   else
#       define ISA_SIGNING_SIGN_MODE ISA_SIGNING_SIGN_NONE
#   endif
#else
#   if __has_feature(ptrauth_objc_isa)
#       define ISA_SIGNING_AUTH_MODE ISA_SIGNING_AUTH
#       define ISA_SIGNING_SIGN_MODE ISA_SIGNING_SIGN_ALL
#   else
#       define ISA_SIGNING_AUTH_MODE ISA_SIGNING_STRIP
#       define ISA_SIGNING_SIGN_MODE ISA_SIGNING_SIGN_NONE
#   endif
#endif

// When set, an unsigned superclass pointer is treated as Nil, which
// will treat the class as if its superclass was weakly linked and
// not loaded, and cause uses of the class to resolve to Nil.
#define SUPERCLASS_SIGNING_TREAT_UNSIGNED_AS_NIL 0

/// ARM64 iOS真机
#if defined(__arm64__) && TARGET_OS_IOS && !TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST
#define CONFIG_USE_PREOPT_CACHES 1
#else
#define CONFIG_USE_PREOPT_CACHES 0
#endif

// When set to 1, small methods in the shared cache have a direct
// offset to a selector. When set to 0, small methods in the shared
// cache have the same format as other small methods, with an offset
// to a selref.
#define CONFIG_SHARED_CACHE_RELATIVE_DIRECT_SELECTORS 1

#endif
