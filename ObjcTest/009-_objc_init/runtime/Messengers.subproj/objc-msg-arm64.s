/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2011 Apple Inc.  All Rights Reserved.
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
/********************************************************************
 * 
 *  objc-msg-arm64.s - ARM64 code to support objc messaging
 *
 ********************************************************************/

#ifdef __arm64__

#include <arm/arch.h>
#include "isa.h"
#include "objc-config.h"
#include "arm64-asm.h"



#if TARGET_OS_IPHONE && __LP64__
	.section	__TEXT,__objc_methname,cstring_literals
l_MagicSelector: /* the shared cache builder knows about this value */
        .byte	0xf0, 0x9f, 0xa4, 0xaf, 0

	.section	__DATA,__objc_selrefs,literal_pointers,no_dead_strip
	.p2align	3
_MagicSelRef:
	.quad	l_MagicSelector
#endif

// 汇编程序中以 . 开头的名称并不是指令的助记符，不会被翻译成机器指令，
// 而是给汇编器一些特殊提示，
// 称为汇编指示（Assembler Directive）或伪操作（Pseudo-operation），
// 由于它不是真正的指令所以加个 "伪" 字。

// .section 指示把代码划分成若干个区（Section），
// 程序被操作系统加载执行时，每个段被加载到不同的地址，
// 操作系统对不同的页面设置不同的读、写、执行权限。

// .section .data
// .data 段保存程序的数据，是可读可写的，相当于 C 程序的全局变量。

// .section .text
// .text 段保存代码，是只读和可执行的，后面那些指令都属于 .text 段。

// .section 分段，可以通过 .section 伪操作来自定义一个段
// .section expr; // expr 可以是 .text/.data/.bss
// .text 将定义符开始的代码编译到代码段
// .data 将定义符开始的数据编译到数据段
// .bss 将变量存放到 .bss 段，bss 段通常是指
// 用来存放程序中未初始化的全局变量的一块内存区域，
// 数据段通常是指用来存放程序中已初始化的全局变量的一块内存区域
// 注意：源程序中 .bss 段应该在 .text 之前

.data  // 表示将定义符开始的数据编译到数据段

// _objc_restartableRanges is used by method dispatch
// caching code to figure out whether any threads are actively 
// in the cache for dispatching.  The labels surround the asm code
// that do cache lookups.  The tables are zero-terminated.

// 方法调度缓存代码使用 _objc_restartableRanges
// 来确定是否有任何线程在 缓存 中处于活动状态以进行调度。
// labels 围绕着执行缓存查找的 asm 代码。这些表以零结尾。

.macro RestartableEntry
#if __LP64__
    // 在 arm64 的 64 位操作系统下
   // .quad 定义一个 8 个字节（两 word）的类型
   //（以 L 开头的标签叫本地标签，这些标签只能用于函数内部）
	.quad	LLookupStart$0
#else
	.long	LLookupStart$0 // .long 定义一个 4 个字节的长整型
	.long	0              // 补位 占4字节
#endif
	.short	LLookupEnd$0 - LLookupStart$0  // .short 定义一个 2 个字节的短整型
	.short	LLookupRecover$0 - LLookupStart$0
	.long	0                              // 补位
.endmacro

	.align 4  // 表示以 2^4 16 字节对齐
	.private_extern _objc_restartableRanges
_objc_restartableRanges:
    // 定义 5 个私有的 RestartableEntry，看名字可以对应到我们日常消息发送中使用到的函数
    // 这里可以理解为 C 语言中的函数声明，它们的实现都在下面，等下我们一行一行来解读
	RestartableEntry _cache_getImp
	RestartableEntry _objc_msgSend
	RestartableEntry _objc_msgSendSuper2
	RestartableEntry _objc_msgLookup
	RestartableEntry _objc_msgLookupSuper2

   // .fill repeat, size, value 含义是反复拷贝 size 个字节，重复 repeat 次，
   // 其中 size 和 value 是可选的，默认值分别是 1 和 0
   // 全部填充 0
	.fill	16, 1, 0

// 下面是 C 的宏定义，C 与 汇编混编

/* objc_super parameter to sendSuper */
// 这里的注释 objc_super 在 Public Header/message.h 中有其定义
// struct objc_super, 有两个成员变量, id receiver 和 Class super_class/ Class class
// __SIZEOF_POINTER__  一个指针的大小 8个字节
#define RECEIVER         0
#define CLASS            __SIZEOF_POINTER__

/* Selected field offsets in class structure */
/* 定义字段 在 class 结构体中的偏移量 */
// 这里说的是 objc_class 结构体的成员，
// 我们知道它的第一个成员变量是继承自 objc_object 的 isa_t isa
// 然后是 Class superclass、cache_t cache
// 这里刚好对应下面的 superclass 偏移 8 个字节，然后 cache 偏移 16 个字节
#define SUPERCLASS       __SIZEOF_POINTER__
#define CACHE            (2 * __SIZEOF_POINTER__)

/* Selected field offsets in method structure */
/* 定义字段 在 method 结构体中的偏移量
    这里对应 method_t 结构体，它有 3 个成员变量：
    SEL name、const char *types、MethodListIMP imp
    name 偏移 0，(SEL 实际类型是 unsigned long 占 8 个字节，所以 types 成员变量偏移是 8)
    types 偏移是 8 (types 实际类型是 const char * 占 8 个字节，所以 imp 成员变量偏移是 16)
    imp 偏移是 2 * 8
 */
#define METHOD_NAME      0
#define METHOD_TYPES     __SIZEOF_POINTER__
#define METHOD_IMP       (2 * __SIZEOF_POINTER__)

// BUCKET_SIZE 宏定义是 bucket_t 的大小，它有两个成员变量 _imp 和 _sel 分别占 8 个字节，所以这里是 16 个字节
#define BUCKET_SIZE      (2 * __SIZEOF_POINTER__)


/********************************************************************
 * GetClassFromIsa_p16 src, needs_auth, auth_address
 * src is a raw isa field. Sets p16 to the corresponding class pointer.
 * The raw isa might be an indexed isa to be decoded, or a
 * packed isa that needs to be masked.
 
 src 是一个原始的 isa 字段。将 p16 设置为相应的类指针。
 从非指针的 isa 中获取类信息时，一种是通过掩码直接从相应位中获取类的指针，
 一种是从相应位中获取类的索引然后在全局的类表中再获取对应的类
 
 *
 * On exit:
 *   src is unchanged
 *   p16 is a class pointer
 *   x10 is clobbered
 退出时：$0(宏定义入参 isa) 不改变，p16 保存一个类指针 x10 是clobbered
 ********************************************************************/

// SUPPORT_INDEXED_ISA 在 x86_64 和 arm64 都不支持
// 主要在 watchOS 中使用（__arm64__ && !__LP64__）（armv7k or arm64_32）
#if SUPPORT_INDEXED_ISA
// 如果优化的 isa 中存放的是 indexcls
	.align 3 // 以 2^3 = 8 字节对齐
	.globl _objc_indexed_classes // 定义一个全局的标记 _objc_indexed_classes
_objc_indexed_classes:

// PTRSIZE 定义在 arm64-asm.h 中，在 arm64 下是8, 在arm64_32 下是 4，
// 表示一个指针的宽度，8 个字节或者 4 个字节
// ISA_INDEX_COUNT 定义在 isa.h 中

// #define ISA_INDEX_BITS 15
// #define ISA_INDEX_COUNT (1 << ISA_INDEX_BITS) // 1 左移 15 位

// uintptr_t nonpointer        : 1;
// uintptr_t has_assoc         : 1;
// uintptr_t indexcls          : 15;
// ...
// indexcls 是第 1-15 位

// .fill repeat, size, value 含义是反复拷贝 size 个字节，重复 repeat 次，
// 其中 size 和 value 是可选的，默认值分别是 1 和 0
// 全部填充 0
	.fill ISA_INDEX_COUNT, PTRSIZE, 0
#endif

///从 isa 中获取类指针并放在通用寄存器 p16 上。
.macro GetClassFromIsa_p16 src, needs_auth, auth_address /* note: auth_address is not required if !needs_auth */

// 以下分别针对我们熟知的三种情况
// 1. isa 中以掩码形式保存的是类的索引
// 2. isa 中以掩码形式保存的是类的指针
// 3. isa 中就是原始的类指针

#if SUPPORT_INDEXED_ISA
	// Indexed isa
// 如果 isa 中存放的是类索引

// 把 src 设置给 p16，这个 $0 是 isa_t/Class isa
	mov	p16, \src			// optimistically set dst = src
// #define ISA_INDEX_IS_NPI_BIT  0
    // 定义在 isa.h 中
    
    // p16[0] 与 1f 进行比较，这里正是对我们的 ISA_BITFIELD 中
    // uintptr_t nonpointer : 1;
    // 标识位进行比较，如果值是 1 则表示是优化的 isa，如果不是则表示是原始指针
    
    // TBNZ X1，#3 label // 若 X1[3] != 0，则跳转到 label
    // TBZ X1，#3 label // 若 X1[3]==0，则跳转到 label
    
    // 如果 p16[0] != 1 的话，表示现在 p16 中保存的不是非指针的 isa，则直接结束宏定义
	tbz	p16, #ISA_INDEX_IS_NPI_BIT, 1f	// done if not non-pointer isa
	// isa in p16 is indexed
    // p16 中的 isa
    
    // 下面的操作大概是根据 isa 中的索引从全局的类表中找到类指针吗？
    
    // ADR
    // 作用：小范围的地址读取指令。ADR 指令将基于 PC 相对偏移的地址值读取到寄存器中。
    // 原理：将有符号的 21 位的偏移，加上 PC,
    // 结果写入到通用寄存器，可用来计算 +/- 1MB 范围的任意字节的有效地址。
    
    // ADRP
    // 作用：以页为单位的大范围的地址读取指令，这里的 P 就是 page 的意思。
    // 通俗来讲，ADRP 指令就是先进行 PC+imm（偏移值）然后找到 lable 所在的一个 4KB 的页，
    // 然后取得 label 的基址，再进行偏移去寻址。
    
    // 将 _objc_indexed_classes 所在的页的基址读入 x10 寄存器
	adrp	x10, _objc_indexed_classes@PAGE

    // x10 = x10 + _objc_indexed_classes(page 中的偏移量)
   // x10 基址根据偏移量进行内存偏移
	add	x10, x10, _objc_indexed_classes@PAGEOFF

    // 无符号位域提取指令
    // UBFX Wd, Wn, #lsb, #width ; 32-bit
    // UBFX Xd, Xn, #lsb, #width ; 64-bit
    // 作用：从 Wn 寄存器的第 lsb 位开始，提取 width 位到 Wd 寄存器，剩余高位用 0 填充
    
    // #define ISA_INDEX_SHIFT 2
    // #define ISA_INDEX_BITS 15
    
    // 从 p16 的第 ISA_INDEX_SHIFT 位开始，
    // 提取 ISA_INDEX_BITS 位到 p16 寄存器，其它位用 0 填充
    // 即从位域中提出 indexcls
	ubfx	p16, p16, #ISA_INDEX_SHIFT, #ISA_INDEX_BITS  // extract index

    // __LP64__ 下: #define PTRSHIFT 3  // 1<<PTRSHIFT == PTRSIZE 2^3 #define PTRSIZE 8
    // !__LP64_ 下: #define PTRSHIFT 2  // 1<<PTRSHIFT == PTRSIZE 2^2 #define PTRSIZE 4
    
    // __LP64__: #define UXTP UXTX
    // !__LP64__: #define UXTP UXTW
    // 扩展指令, 扩展 p16 左移 8/4 位
    // 然后是从 x10 开始偏对应的位，然后把此处的值存储到 p16 中去。
    // 暂不明白为什么这样就可以找到类了，还有全局的类表是存在哪里的呢 ？
    
    // 从数组加载类到 p16 中
	ldr	p16, [x10, p16, UXTP #PTRSHIFT]	// load class from array
1:

#elif __LP64__
.if \needs_auth == 0 // _cache_getImp takes an authed class already
// 直接把 isa 放入 p16 中
	mov	p16, \src
.else
	// 64-bit packed isa
    // 实际是 and  $0, $1, #ISA_MASK
    // 如果 class pointer 保存在 isa 中
    // #define ISA_MASK 0x0000000ffffffff8ULL
    // ISA_MASK 和 $0(isa) 做与运算提取出其中的 class pointer 放在 p16 中
	ExtractISA p16, \src, \auth_address
.endif
#else
    // 最后一种情况，isa 就是原始的类指针
   
   // 32-bit raw isa
   // 直接把 isa 放入 p16 中
	mov	p16, \src

#endif

.endmacro


/********************************************************************
 * ENTRY functionName
 * STATIC_ENTRY functionName
 * END_ENTRY functionName
 
  定义一个汇编宏 ENTRY，表示在 text 段定义一个 32 字节对齐的 global 函数，
  "$0" 同时生产一个函数入口标签。
  $0 表示宏定义的第一个入参
 ********************************************************************/

.macro ENTRY /* name */
	.text  // .text 定义一个代码段，处理器开始执行代码的时候，代表后面是代码。这是 GCC 必须的。
	.align 5  // 2^5，32 个字节对齐
	.globl    $0  // .global 关键字用来让一个符号对链接器可见，可以供其他链接对象模块使用，
                 // 告诉汇编器后续跟的是一个全局可见的名字（可能是变量，也可以是函数名）
                 
                 // 这里用来指定 $0，$0 代表入参，
                 // 是不是就是表示 ENTRY 标注的函数都是全局可见的函数
                 
                 // 00001:
                 // 00002: .text
                 // 00003: .global _start
                 // 00004:
                 // 00005: _start:
                 
                 // .global _start 和 _start: 配合，
                 // 给代码开始地址定义一个全局标记 _start。
                 // _start 是一个函数的起始地址，也是编译、链接后程序的起始地址。
                 // 由于程序是通过加载器来加载的，
                 // 必须要找到 _start 名字的的函数，因此 _start 必须定义成全局的，
                 // 以便存在于编译后的全局符号表中，
                 // 供其他程序（如加载器）寻找到。
                 
                 // .global _start 让 _start 符号成为可见的标示符，
                 // 这样链接器就知道跳转到程序中的什么地方并开始执行，
                 // Linux 寻找这个 _start 标签作为程序的默认进入点。
                 
                 // .extern xxx 说明 xxx 为外部函数，
                 // 调用的时候可以遍访所有文件找到该函数并且使用它
$0:
.endmacro

// 同上
.macro STATIC_ENTRY /*name*/
	.text
	.align 5
	.private_extern $0
$0:
.endmacro

// END_ENTRY entry 结束
.macro END_ENTRY /* name */
LExit$0: // 只有一个 LExit$0 标签 （以 L 开头的标签叫本地标签，这些标签只能用于函数内部）
.endmacro


/********************************************************************
 * UNWIND name, flags
 * Unwind info generation	
 ********************************************************************/
.macro UNWIND
	.section __LD,__compact_unwind,regular,debug

    // __LP64__: #define PTR .quad
    // !__LP64__: #define PTR .long

	PTR $0  // .quad 定义 8 个字节（两 word）的类型 / .long 定义 4 个字节的长整型
	.set  LUnwind$0, LExit$0 - $0 // .set 给一个 全局变量或局部变量 赋值

    // .long 定义 4 个字节的长整型 （以 L 开头的标签叫本地标签，这些标签只能用于函数内部）
	.long LUnwind$0
	.long $1
// .quad 定义 8 个字节（两 word）的类型
	PTR 0	 /* no personality */
// .quad 定义 8 个字节（两 word）的类型
	PTR 0  /* no LSDA */
	.text // .text 定义一个代码段，处理器开始执行代码的时候，代表后面是代码。这是 GCC 必须的。
.endmacro

// 硬编码定值
#define NoFrame 0x02000000  // no frame, no SP adjustment
#define FrameWithNoSaves 0x04000000  // frame, no non-volatile saves


#define MSGSEND 100
#define METHOD_INVOKE 101

//////////////////////////////////////////////////////////////////////
//
// SAVE_REGS
//
// Create a stack frame and save all argument registers in preparation
// for a function call.
//////////////////////////////////////////////////////////////////////

.macro SAVE_REGS kind

    // push frame
    SignLR
    stp    fp, lr, [sp, #-16]!
    mov    fp, sp

    // save parameter registers: x0..x8, q0..q7
    // 保存方法参数到寄存器中
    sub    sp, sp,  #(10*8 + 8*16)
    stp    q0, q1,  [sp, #(0*16)]
    stp    q2, q3,  [sp, #(2*16)]
    stp    q4, q5,  [sp, #(4*16)]
    stp    q6, q7,  [sp, #(6*16)]
    stp    x0, x1,  [sp, #(8*16+0*8)]
    stp    x2, x3,  [sp, #(8*16+2*8)]
    stp    x4, x5,  [sp, #(8*16+4*8)]
    stp    x6, x7,  [sp, #(8*16+6*8)]
.if \kind == MSGSEND
    stp    x8, x15, [sp, #(8*16+8*8)]
    mov    x16, x15 // stashed by CacheLookup, restore to x16
.elseif \kind == METHOD_INVOKE
    str    x8,      [sp, #(8*16+8*8)]
.else
.abort Unknown kind.
.endif

.endmacro


//////////////////////////////////////////////////////////////////////
//
// RESTORE_REGS
//
// Restore all argument registers and pop the stack frame created by
// SAVE_REGS.
//////////////////////////////////////////////////////////////////////

.macro RESTORE_REGS kind

    ldp    q0, q1,  [sp, #(0*16)]
    ldp    q2, q3,  [sp, #(2*16)]
    ldp    q4, q5,  [sp, #(4*16)]
    ldp    q6, q7,  [sp, #(6*16)]
    ldp    x0, x1,  [sp, #(8*16+0*8)]
    ldp    x2, x3,  [sp, #(8*16+2*8)]
    ldp    x4, x5,  [sp, #(8*16+4*8)]
    ldp    x6, x7,  [sp, #(8*16+6*8)]
.if \kind == MSGSEND
    ldp    x8, x16, [sp, #(8*16+8*8)]
    orr    x16, x16, #2  // for the sake of instrumentations, remember it was the slowpath
.elseif \kind == METHOD_INVOKE
    ldr    x8,      [sp, #(8*16+8*8)]
.else
.abort Unknown kind.
.endif

    mov    sp, fp
    ldp    fp, lr, [sp], #16
    AuthenticateLR

.endmacro


/********************************************************************
 *
 * CacheLookup NORMAL|GETIMP|LOOKUP <function> MissLabelDynamic MissLabelConstant
    (分别代表三种不同的执行目的，LOOKUP 是进行查找，GETIMP 是获取 IMP, NORMAL 则是正常的找到 IMP 执行并会返回 IMP)
 *
 * MissLabelConstant is only used for the GETIMP variant.

 
 *
 * Locate the implementation for a selector in a class method cache.
   在类方法缓存中找到 selector 的实现。
 *
 * When this is used in a function that doesn't hold the runtime lock,
 * this represents the critical section that may access dead memory.
 当它在不持有 runtime lock 的函数中使用时，它表示可能访问 死内存 的 关键部分。
 * If the kernel causes one of these functions to go down the recovery
 * path, we pretend the lookup failed by jumping the JumpMiss branch.
 如果内核导致这些功能之一沿恢复路径消失，我们将跳过 JumpMiss 分支来假装查找失败。
 *
 * Takes:
 *	 x1 = selector // x1 寄存器存放 selector
 *	 x16 = class to be searched // x16 寄存器中存放 Class
 *
 * Kills:
 * 	 x9,x10,x11,x12,x13,x15,x17
 *
 * Untouched:
 * 	 x14
 *
 * On exit: (found) calls or returns IMP
 *                  with x16 = class, x17 = IMP
 *                  In LOOKUP mode, the two low bits are set to 0x3
 *                  if we hit a constant cache (used in objc_trace)
            如果找到的话，会调用或返回 IMP，x16 中保存类信息，x17 中保存 IMP
 
 *          (not found) jumps to LCacheMiss
 *                  with x15 = class
 *                  For constant caches in LOOKUP mode, the low bit
 *                  of x16 is set to 0x1 to indicate we had to fallback.
            如果未找到的话，跳转到 LCacheMiss, x15保存类的信息
 
 *          In addition, when LCacheMiss is __objc_msgSend_uncached or
 *          __objc_msgLookup_uncached, 0x2 will be set in x16
 *          to remember we took the slowpath.
 *          So the two low bits of x16 on exit mean:
 *            0: dynamic hit
 *            1: fallback to the parent class, when there is a preoptimized cache
 *            2: slowpath
 *            3: preoptimized cache hit
 *
 ********************************************************************/

#define NORMAL 0
#define GETIMP 1
#define LOOKUP 2

// CacheHit: x17 = cached IMP, x10 = address of buckets, x1 = SEL, x16 = isa
// 缓存命中的宏:
// 缓存命中：x17 IMP的地址, x10 buckets 的地址, x1 中存的是SEL, x16 中保存类信息
.macro CacheHit
.if $0 == NORMAL // NORMAL 表示通常情况下在缓存中找到了函数执行并返回
// TailCallCachedImp 定义在 arm64-asm.h 中
// 验证并执行 IMP
	TailCallCachedImp x17, x10, x1, x16	// authenticate and call imp

.elseif $0 == GETIMP // GETIMP 仅在缓存中查找 IMP
// p17 中是 cached IMP，放进 p0 中
	mov	p0, p17
// cbz 比较（Compare），如果结果为零（Zero）就转移（只能跳到后面的指令）
// 如果 p0 是 0，则跳转到 标签 9 处，标签 9 处直接执行 ret
	cbz	p0, 9f			// don't ptrauth a nil imp
	AuthAndResignAsIMP x0, x10, x1, x16	// authenticate imp and re-sign as IMP
// return IMP
9:	ret				// return IMP

.elseif $0 == LOOKUP // LOOKUP 进行查找
	// No nil check for ptrauth: the caller would crash anyway when they
	// jump to a nil IMP. We don't care if that jump also fails ptrauth.

// 不去检测imp是否为nil, 也不关心跳转是否会失败
	AuthAndResignAsIMP x17, x10, x1, x16	// authenticate imp and re-sign as IMP
	cmp	x16, x15
	cinc	x16, x16, ne			// x16 += 1 when x15 != x16 (for instrumentation ; fallback to the parent class)
	ret				// return imp via x17
.else
// .abort 停止汇编
// Linux内核在发生 kernel panic 时会打印出 Oops 信息，
// 把目前的寄存器状态、堆栈内容、以及完整的 Call trace 都 show 给我们看，
// 这样就可以帮助我们定位错误。
.abort oops
.endif
.endmacro// 结束 CacheHit 汇编宏定义

.macro CacheLookup Mode, Function, MissLabelDynamic, MissLabelConstant
	//
	// Restart protocol:
    // 重启协议:
	//
	//   As soon as we're past the LLookupStart\Function label we may have
	//   loaded an invalid cache pointer or mask.
    //   一旦超过 LLookupStart$1 标签，我们可能已经加载了无效的 缓存指针 或 掩码。
	//
	//   When task_restartable_ranges_synchronize() is called,
	//   (or when a signal hits us) before we're past LLookupEnd\Function,
	//   then our PC will be reset to LLookupRecover\Function which forcefully
	//   jumps to the cache-miss codepath which have the following
    //   requirements:
    //   当我们在超过 LLookupEnd$1 之前（或当 信号   命中我们）调用task_restartable_ranges_synchronize()，我们的 PC 将重置为  LLookupRecover$1，这将强制跳转到缓存未命中的代码路径，其中包含以下内容。

	
	//
	//   GETIMP:
	//     The cache-miss is just returning NULL (setting x0 to 0)
    //     缓存未命中只是返回 NULL
	//
	//   NORMAL and LOOKUP:
	//   - x0 contains the receiver // x0 存放函数接收者 (就是我们日常的 self)
	//   - x1 contains the selector // x1 存放 SEL (就是我们日常的 @selector(xxxx))
	//   - x16 contains the isa     // x16 是 class 的 isa (也就是 self 的 isa，根据它来找到对象所属的类)
	//   - other registers are set as per calling conventions // 其它寄存器根据调用约定来设置
	//

//将原始的isa存储到x15
	mov	x15, x16			// stash the original isa
LLookupStart\Function:
	// p1 = SEL, p16 = isa
#if CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS

//#define CACHE            (2 * __SIZEOF_POINTER__)   就是16
//将存储器地址为(x16+16)的字数据读入寄存器 p10
//类的地址偏移16个字节后，刚好是cache_t的起始地址，地址上的数据, 也就是cache_t的第一个成员变量_bucketsAndMaybeMask
//将cache的内容读取到p10
	ldr	p10, [x16, #CACHE]				// p10 = mask|buckets
//p10存储的数据右移48位 后 存入p11, 此时11为mask
	lsr	p11, p10, #48			// p11 = mask
//p10 和 bucketsmask 与运算后 再存入p10  此时p10存的是buckets地址
	and	p10, p10, #0xffffffffffff	// p10 = buckets
//w小端模式 x1是sel x11是mask
//计算后 x12存的的当前sel经过hash计算后得到的key
	and	w12, w1, w11			// x12 = _cmd & mask   哈希计算下标

#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16
	ldr	p11, [x16, #CACHE]			// p11 = mask|buckets
#if CONFIG_USE_PREOPT_CACHES
#if __has_feature(ptrauth_calls)
	tbnz	p11, #0, LLookupPreopt\Function
	and	p10, p11, #0x0000ffffffffffff	// p10 = buckets
#else
	and	p10, p11, #0x0000fffffffffffe	// p10 = buckets
	tbnz	p11, #0, LLookupPreopt\Function
#endif
	eor	p12, p1, p1, LSR #7
	and	p12, p12, p11, LSR #48		// x12 = (_cmd ^ (_cmd >> 7)) & mask
#else
	and	p10, p11, #0x0000ffffffffffff	// p10 = buckets
	and	p12, p1, p11, LSR #48		// x12 = _cmd & mask
#endif // CONFIG_USE_PREOPT_CACHES

#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_LOW_4
	ldr	p11, [x16, #CACHE]				// p11 = mask|buckets
	and	p10, p11, #~0xf			// p10 = buckets
	and	p11, p11, #0xf			// p11 = maskShift
	mov	p12, #0xffff
	lsr	p11, p12, p11			// p11 = mask = 0xffff >> p11
	and	p12, p1, p11			// x12 = _cmd & mask
#else
#error Unsupported cache mask storage for ARM64.
#endif

// 在 Project Headers/arm64-asm.h 中可以看到 PTRSHIFT 的宏定义
/*
    #if __arm64__
    #if __LP64__ // 64 位系统架构
    #define PTRSHIFT 3  // 1<<PTRSHIFT == PTRSIZE // 0b1000 表示一个指针 8 个字节
    // "p" registers are pointer-sized
    // true arm64
    #else
    // arm64_32 // 32 位系统架构
    #define PTRSHIFT 2  // 1<<PTRSHIFT == PTRSIZE // 0b100 表示一个指针 4 个字节
    // "p" registers are pointer-sized
    // arm64_32
    #endif
 */


// p12, LSL #(1+PTRSHIFT) 将p12(key) 逻辑左移4位, 左移相等于 *16(bucket大小), 又由于key相当于bucket在bucket中的下标, 所以这一步就是 计算出内存偏移量
// p10 是buekets的首地址, 与偏移量相加 ,计算出bucket 实际的内存地址, 存到p13
// p13 就是buckets中下标为key的元素的 地址
	add	p13, p10, p12, LSL #(1+PTRSHIFT)
						// p13 = buckets + ((_cmd & mask) << (1+PTRSHIFT))

						// do {
//ldp 出栈指令（`ldr` 的变种指令，可以同时操作两个寄存器）
//将x13偏移BUCKET_SIZE (16) 个字节的内容取出来, 分别存入x17 和 x9
//p17是imp p9是sel
//然后将bucket指针前移一个单位长度
1:	ldp	p17, p9, [x13], #-BUCKET_SIZE	//     {imp, sel} = *bucket--
//比较sel和cmd 比较从buckets中取出的sel和传入的sel是否相等
	cmp	p9, p1				//     if (sel != _cmd) {
//如果不相等, 就跳转到3执行
	b.ne	3f				//         scan more
						//     } else {
//如果相等 缓存命中 跳转到CacheHit执行
2:	CacheHit \Mode				// hit:    call or return imp
						//     }

//如果p9为0 取出的sel为空 未找到缓存 就跳转到 __objc_msgSend_uncached执行
3:	cbz	p9, \MissLabelDynamic		//     if (sel == 0) goto Miss;
//比较bucket地址和buckets地址
//当bucket >= buckets 还成立时 跳回1 继续执行 哈希探测
	cmp	p13, p10			// } while (bucket >= buckets)
	b.hs	1b

	// wrap-around:
	//   p10 = first bucket
	//   p11 = mask (and maybe other bits on LP64)
	//   p12 = _cmd & mask
	//
	// A full cache can happen with CACHE_ALLOW_FULL_UTILIZATION.
	// So stop when we circle back to the first probed bucket
	// rather than when hitting the first bucket again.
	//
	// Note that we might probe the initial bucket twice
	// when the first probed slot is the last entry.


#if CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS
	add	p13, p10, w11, UXTW #(1+PTRSHIFT)
						// p13 = buckets + (mask << 1+PTRSHIFT)
#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16
	add	p13, p10, p11, LSR #(48 - (1+PTRSHIFT))
						// p13 = buckets + (mask << 1+PTRSHIFT)
						// see comment about maskZeroBits
#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_LOW_4
	add	p13, p10, p11, LSL #(1+PTRSHIFT)
						// p13 = buckets + (mask << 1+PTRSHIFT)
#else
#error Unsupported cache mask storage for ARM64.
#endif

//再查找一遍
	add	p12, p10, p12, LSL #(1+PTRSHIFT)
						// p12 = first probed bucket

						// do {
4:	ldp	p17, p9, [x13], #-BUCKET_SIZE	//     {imp, sel} = *bucket--
	cmp	p9, p1				//     if (sel == _cmd)
	b.eq	2b				//         goto hit
	cmp	p9, #0				// } while (sel != 0 &&
	ccmp	p13, p12, #0, ne		//     bucket > first_probed)
	b.hi	4b

LLookupEnd\Function:
//最后强制跳转到cache miss
LLookupRecover\Function:
	b	\MissLabelDynamic

#if CONFIG_USE_PREOPT_CACHES
#if CACHE_MASK_STORAGE != CACHE_MASK_STORAGE_HIGH_16
#error config unsupported
#endif
LLookupPreopt\Function:
#if __has_feature(ptrauth_calls)
	and	p10, p11, #0x007ffffffffffffe	// p10 = buckets
	autdb	x10, x16			// auth as early as possible
#endif

	// x12 = (_cmd - first_shared_cache_sel)
	adrp	x9, _MagicSelRef@PAGE
	ldr	p9, [x9, _MagicSelRef@PAGEOFF]
	sub	p12, p1, p9

	// w9  = ((_cmd - first_shared_cache_sel) >> hash_shift & hash_mask)
#if __has_feature(ptrauth_calls)
	// bits 63..60 of x11 are the number of bits in hash_mask
	// bits 59..55 of x11 is hash_shift

	lsr	x17, x11, #55			// w17 = (hash_shift, ...)
	lsr	w9, w12, w17			// >>= shift

	lsr	x17, x11, #60			// w17 = mask_bits
	mov	x11, #0x7fff
	lsr	x11, x11, x17			// p11 = mask (0x7fff >> mask_bits)
	and	x9, x9, x11			// &= mask
#else
	// bits 63..53 of x11 is hash_mask
	// bits 52..48 of x11 is hash_shift
	lsr	x17, x11, #48			// w17 = (hash_shift, hash_mask)
	lsr	w9, w12, w17			// >>= shift
	and	x9, x9, x11, LSR #53		// &=  mask
#endif

	ldr	x17, [x10, x9, LSL #3]		// x17 == sel_offs | (imp_offs << 32)
	cmp	x12, w17, uxtw

.if \Mode == GETIMP
	b.ne	\MissLabelConstant		// cache miss
	sub	x0, x16, x17, LSR #32		// imp = isa - imp_offs
	SignAsImp x0
	ret
.else
	b.ne	5f				// cache miss
	sub	x17, x16, x17, LSR #32		// imp = isa - imp_offs
.if \Mode == NORMAL
	br	x17
.elseif \Mode == LOOKUP
	orr x16, x16, #3 // for instrumentation, note that we hit a constant cache
	SignAsImp x17
	ret
.else
.abort  unhandled mode \Mode
.endif

5:	ldursw	x9, [x10, #-8]			// offset -8 is the fallback offset
	add	x16, x16, x9			// compute the fallback isa
	b	LLookupStart\Function		// lookup again with a new isa
.endif
#endif // CONFIG_USE_PREOPT_CACHES

.endmacro


/********************************************************************
 *
 * id objc_msgSend(id self, SEL _cmd, ...);
 * IMP objc_msgLookup(id self, SEL _cmd, ...);
 函数声明，两个函数入参一样，一个是执行函数并返回 id 返回值。
 一个则是在 self 中找到指定 SEL 的 IMP。
 * 
 * objc_msgLookup ABI:
 * IMP returned in x17  找到的 IMP 保存在 x17 寄存器中
 * x16 reserved for our use but not used  x16 寄存器则是保留寄存器
 *
 ********************************************************************/

#if SUPPORT_TAGGED_POINTERS
	.data      // 数据内容
	.align 3   // 2^3 = 8 字节对齐

    // 定义一个全局的标记 _objc_debug_taggedpointer_classes
	.globl _objc_debug_taggedpointer_ext_classes
_objc_debug_taggedpointer_ext_classes:

    // .fill repeat, size, value 含义是反复拷贝 size 个字节，重复 repeat 次，
    // 其中 size 和 value 是可选的，默认值分别是 1 和 0
    // 全部填充 0
	.fill 256, 8, 0

// Dispatch for split tagged pointers take advantage of the fact that
// the extended tag classes array immediately precedes the standard
// tag array. The .alt_entry directive ensures that the two stay
// together. This is harmless when using non-split tagged pointers.
    // 定义一个全局标记 _objc_debug_taggedpointer_ext_classes
	.globl _objc_debug_taggedpointer_classes
	.alt_entry _objc_debug_taggedpointer_classes
_objc_debug_taggedpointer_classes:
	.fill 16, 8, 0

// Look up the class for a tagged pointer in x0, placing it in x16.
.macro GetTaggedClass

	and	x10, x0, #0x7		// x10 = small tag
	asr	x11, x0, #55		// x11 = large tag with 1s filling the top (because bit 63 is 1 on a tagged pointer)
	cmp	x10, #7		// tag == 7?
	csel	x12, x11, x10, eq	// x12 = index in tagged pointer classes array, negative for extended tags.
					// The extended tag array is placed immediately before the basic tag array
					// so this looks into the right place either way. The sign extension done
					// by the asr instruction produces the value extended_tag - 256, which produces
					// the correct index in the extended tagged pointer classes array.

	// x16 = _objc_debug_taggedpointer_classes[x12]
	adrp	x10, _objc_debug_taggedpointer_classes@PAGE
	add	x10, x10, _objc_debug_taggedpointer_classes@PAGEOFF
	ldr	x16, [x10, x12, LSL #3]

.endmacro
#endif


    /*
    .macro ENTRY  //name
     .text
     .align 5
     .globl    $0
     $0:
     .endmacro
    
     $0 表示是 _objc_msgSend
     那么整体的含义就是：
     _objc_msgSend 是一个代码段，然后是 2^5 = 32 个字节对齐
     然后用 global 修饰，大概可以理解是一个全局函数.
   */

//_objc_msgSend 方法入口
	ENTRY _objc_msgSend
	UNWIND _objc_msgSend, NoFrame

    // p0 和 0 比较，即判断接收者是否存在，
    // 其中 p0 是 objc_msgSend 的第一个参数，消息接收者 receiver
	cmp	p0, #0			// nil check and tagged pointer check
#if SUPPORT_TAGGED_POINTERS //
// 支持 tagged pointer 的流程, 并且比较的结果 le 小于或等于
// 跳转到 LNilOrTagged 标签处执行 Taggend Pointer 对象的函数查找及执行
	b.le	LNilOrTagged		//  (MSB tagged pointer looks negative)
#else
// p0 等于 0 的话，则跳转到 LReturnZero 标签处
// LReturnZero 置 0 返回 nil 并直接结束 _objc_msgSend 函数
	b.eq	LReturnZero
#endif

// 不过方法接收者不为nil 就会继续向下执行
// p0 即 receiver 肯定存在的流程，实际规定是 p0 - p7 是接收函数参数的寄存器
// 从 x0 寄存器指向的地址取出 isa，存入 p13 寄存器
	ldr	p13, [x0]		//
// 从 isa 中获取类指针并存放在通用寄存器 p16 中
	GetClassFromIsa_p16 p13, 1, x0	// p16 = class

// 本地标签（表示获得 isa 完成）
LGetIsaDone:
	// calls imp or objc_msgSend_uncached
// 如果有 isa，走到 CacheLookup 即缓存查找流程，也就是所谓的 sel-imp 快速查找流程，
	CacheLookup NORMAL, _objc_msgSend, __objc_msgSend_uncached

#if SUPPORT_TAGGED_POINTERS
LNilOrTagged:
// nil 检测，如果是 nil 的话也跳转到 LReturnZero 标签处
	b.eq	LReturnZero		// nil check
//从tagged pointer指针中查找class, 并存放到x16中
	GetTaggedClass
	b	LGetIsaDone
// SUPPORT_TAGGED_POINTERS
#endif

LReturnZero:
	// x0 is already zero
    // 置 0
	mov	x1, #0
	movi	d0, #0
	movi	d1, #0
	movi	d2, #0
	movi	d3, #0
    // return 结束执行
	ret

    // LExit 结束 _objc_msgSend 函数执行
    END_ENTRY _objc_msgSend


	ENTRY _objc_msgLookup
	UNWIND _objc_msgLookup, NoFrame
	cmp	p0, #0			// nil check and tagged pointer check
#if SUPPORT_TAGGED_POINTERS
	b.le	LLookup_NilOrTagged	//  (MSB tagged pointer looks negative)
#else
	b.eq	LLookup_Nil
#endif
	ldr	p13, [x0]		// p13 = isa
	GetClassFromIsa_p16 p13, 1, x0	// p16 = class
LLookup_GetIsaDone:
	// returns imp
	CacheLookup LOOKUP, _objc_msgLookup, __objc_msgLookup_uncached

#if SUPPORT_TAGGED_POINTERS
LLookup_NilOrTagged:
	b.eq	LLookup_Nil	// nil check
	GetTaggedClass
	b	LLookup_GetIsaDone
// SUPPORT_TAGGED_POINTERS
#endif

LLookup_Nil:
	adr	x17, __objc_msgNil
	SignAsImp x17
	ret

	END_ENTRY _objc_msgLookup

	
	STATIC_ENTRY __objc_msgNil

	// x0 is already zero
	mov	x1, #0
	movi	d0, #0
	movi	d1, #0
	movi	d2, #0
	movi	d3, #0
	ret
	
	END_ENTRY __objc_msgNil


	ENTRY _objc_msgSendSuper
	UNWIND _objc_msgSendSuper, NoFrame

	ldp	p0, p16, [x0]		// p0 = real receiver, p16 = class
	b L_objc_msgSendSuper2_body

	END_ENTRY _objc_msgSendSuper

	// no _objc_msgLookupSuper

	ENTRY _objc_msgSendSuper2
	UNWIND _objc_msgSendSuper2, NoFrame

#if __has_feature(ptrauth_calls)
	ldp	x0, x17, [x0]		// x0 = real receiver, x17 = class
	add	x17, x17, #SUPERCLASS	// x17 = &class->superclass
	ldr	x16, [x17]		// x16 = class->superclass
	AuthISASuper x16, x17, ISA_SIGNING_DISCRIMINATOR_CLASS_SUPERCLASS
LMsgSendSuperResume:
#else
	ldp	p0, p16, [x0]		// p0 = real receiver, p16 = class
	ldr	p16, [x16, #SUPERCLASS]	// p16 = class->superclass
#endif
L_objc_msgSendSuper2_body:
	CacheLookup NORMAL, _objc_msgSendSuper2, __objc_msgSend_uncached

	END_ENTRY _objc_msgSendSuper2

	
	ENTRY _objc_msgLookupSuper2
	UNWIND _objc_msgLookupSuper2, NoFrame

#if __has_feature(ptrauth_calls)
	ldp	x0, x17, [x0]		// x0 = real receiver, x17 = class
	add	x17, x17, #SUPERCLASS	// x17 = &class->superclass
	ldr	x16, [x17]		// x16 = class->superclass
	AuthISASuper x16, x17, ISA_SIGNING_DISCRIMINATOR_CLASS_SUPERCLASS
LMsgLookupSuperResume:
#else
	ldp	p0, p16, [x0]		// p0 = real receiver, p16 = class
	ldr	p16, [x16, #SUPERCLASS]	// p16 = class->superclass
#endif
	CacheLookup LOOKUP, _objc_msgLookupSuper2, __objc_msgLookup_uncached

	END_ENTRY _objc_msgLookupSuper2


.macro MethodTableLookup
	//保存方法参数到寄存器中
	SAVE_REGS MSGSEND

	// lookUpImpOrForward(obj, sel, cls, LOOKUP_INITIALIZE | LOOKUP_RESOLVER)
	// receiver and selector already in x0 and x1
    // 位移 为调用_lookUpImpOrForward方法准备参数
    // receiver 和 selector此时已经存储到x0 和 x1 了

    //将class作为第三个参数
	mov	x2, x16
    //第四个参数传递 3
	mov	x3, #3
    
    //bl bl
    // 如果缓存中未找到，则跳转到 _lookUpImpOrForward（c 函数） 去方法列表中去找函数，
	bl	_lookUpImpOrForward

	// IMP in x0
	mov	x17, x0

    // 恢复寄存器并返回
	RESTORE_REGS MSGSEND

.endmacro

	STATIC_ENTRY __objc_msgSend_uncached
	UNWIND __objc_msgSend_uncached, FrameWithNoSaves

	// THIS IS NOT A CALLABLE C FUNCTION
	// Out-of-band p15 is the class to search
	
	MethodTableLookup
/*
 .macro TailCallFunctionPointer
     // $0 = function pointer value
     braaz    $0
 .endmacro
 */
	TailCallFunctionPointer x17

	END_ENTRY __objc_msgSend_uncached


	STATIC_ENTRY __objc_msgLookup_uncached
	UNWIND __objc_msgLookup_uncached, FrameWithNoSaves

	// THIS IS NOT A CALLABLE C FUNCTION
	// Out-of-band p15 is the class to search
	
	MethodTableLookup
	ret

	END_ENTRY __objc_msgLookup_uncached


	STATIC_ENTRY _cache_getImp

	GetClassFromIsa_p16 p0, 0
	CacheLookup GETIMP, _cache_getImp, LGetImpMissDynamic, LGetImpMissConstant

LGetImpMissDynamic:
	mov	p0, #0
	ret

LGetImpMissConstant:
	mov	p0, p2
	ret

	END_ENTRY _cache_getImp


/********************************************************************
*
* id _objc_msgForward(id self, SEL _cmd,...);
*
* _objc_msgForward is the externally-callable
*   function returned by things like method_getImplementation().
* _objc_msgForward_impcache is the function pointer actually stored in
*   method caches.
*
********************************************************************/

	STATIC_ENTRY __objc_msgForward_impcache

	// No stret specialization.
	b	__objc_msgForward

	END_ENTRY __objc_msgForward_impcache

	
	ENTRY __objc_msgForward

	adrp	x17, __objc_forward_handler@PAGE
	ldr	p17, [x17, __objc_forward_handler@PAGEOFF]
	TailCallFunctionPointer x17
	
	END_ENTRY __objc_msgForward
	
	
	ENTRY _objc_msgSend_noarg
	b	_objc_msgSend
	END_ENTRY _objc_msgSend_noarg

	ENTRY _objc_msgSend_debug
	b	_objc_msgSend
	END_ENTRY _objc_msgSend_debug

	ENTRY _objc_msgSendSuper2_debug
	b	_objc_msgSendSuper2
	END_ENTRY _objc_msgSendSuper2_debug

	
	ENTRY _method_invoke

	// See if this is a small method.
	tbnz	p1, #0, L_method_invoke_small

	// We can directly load the IMP from big methods.
	// x1 is method triplet instead of SEL
	add	p16, p1, #METHOD_IMP
	ldr	p17, [x16]
	ldr	p1, [x1, #METHOD_NAME]
	TailCallMethodListImp x17, x16

L_method_invoke_small:
	// Small methods require a call to handle swizzling.
	SAVE_REGS METHOD_INVOKE
	mov	p0, p1
	bl	__method_getImplementationAndName
	// ARM64_32 packs both return values into x0, with SEL in the high bits and IMP in the low.
	// ARM64 just returns them in x0 and x1.
	mov	x17, x0
#if __LP64__
	mov	x16, x1
#endif
	RESTORE_REGS METHOD_INVOKE
#if __LP64__
	mov	x1, x16
#else
	lsr	x1, x17, #32
	mov	w17, w17
#endif
	TailCallFunctionPointer x17

	END_ENTRY _method_invoke

#endif
