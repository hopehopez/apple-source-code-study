/*
 * Copyright (c) 2008-2013 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

/*
 * IMPORTANT: This header file describes INTERNAL interfaces to libdispatch
 * which are subject to change in future releases of Mac OS X. Any applications
 * relying on these interfaces WILL break.
 */

#ifndef __DISPATCH_SEMAPHORE_INTERNAL__
#define __DISPATCH_SEMAPHORE_INTERNAL__

struct dispatch_queue_s;

/* dispatch_semaphore_s结构体全部展开后
struct dispatch_semaphore_s;

// OS_OBJECT_CLASS_DECL(dispatch_semaphore, DISPATCH_OBJECT_VTABLE_HEADER(dispatch_semaphore))

struct dispatch_semaphore_extra_vtable_s {
    unsigned long const do_type;
    void (*const do_dispose)(struct dispatch_semaphore_s *, bool *allow_free);
    size_t (*const do_debug)(struct dispatch_semaphore_s *, char *, size_t);
    void (*const do_invoke)(struct dispatch_semaphore_s *, dispatch_invoke_context_t, dispatch_invoke_flags_t);
};

struct dispatch_semaphore_vtable_s {
    // _OS_OBJECT_CLASS_HEADER();
    void (*_os_obj_xref_dispose)(_os_object_t);
    void (*_os_obj_dispose)(_os_object_t);
    
    struct dispatch_semaphore_extra_vtable_s _os_obj_vtable;
};

// OS_OBJECT_CLASS_SYMBOL(dispatch_semaphore)

extern const struct dispatch_semaphore_vtable_s _OS_dispatch_semaphore_vtable;
extern const struct dispatch_semaphore_vtable_s OS_dispatch_semaphore_class __asm__("__" OS_STRINGIFY(dispatch_semaphore) "_vtable");

struct dispatch_semaphore_s {
    struct dispatch_object_s _as_do[0];
    struct _os_object_s _as_os_obj[0];
    
	// must be pointer-sized
    const struct dispatch_semaphore_vtable_s *do_vtable; 
    
    int volatile do_ref_cnt;
    int volatile do_xref_cnt;
    
    struct dispatch_semaphore_s *volatile do_next;
    struct dispatch_queue_s *do_targetq;
    void *do_ctxt;
    void *do_finalizer;
    
    // 可看到上半部分和其它 GCD 对象都是相同的，毕竟大家都是继承自 dispatch_object_s，重点是下面两个新的成员变量
    // dsema_value 和 dsema_orig 是信号量执行任务的关键，执行一次 dispatch_semaphore_wait 操作，dsema_value 的值就做一次减操作
    
    long volatile dsema_value;
    long dsema_orig;
    _dispatch_sema4_t dsema_sema;
};

*/

/*
dispatch_semaphore 是 GCD 中最常见的操作，通常用于保证资源的多线程安全性和控制任务的并发数量。
其本质实际上是基于 mach 内核的信号量接口来实现的。
*/

DISPATCH_CLASS_DECL(semaphore, OBJECT);
struct dispatch_semaphore_s {
	DISPATCH_OBJECT_HEADER(semaphore);
	intptr_t volatile dsema_value;
	intptr_t dsema_orig;
	_dispatch_sema4_t dsema_sema;
};


/*
 * Dispatch Group State:
 *
 * Generation (32 - 63):
 *   32 bit counter that is incremented each time the group value reaaches
 *   0 after a dispatch_group_leave. This 32bit word is used to block waiters
 *   (threads in dispatch_group_wait) in _dispatch_wait_on_address() until the
 *   generation changes.
 *   每次组值在 dispatch_group_leave 后达到 0 时递增的 32 位计数器。
 * 这个 32 位字用于阻塞 _dispatch_wait_on_address() 中的等待者（dispatch_group_wait 中的线程），直到代发生变化。
 *
 * Value (2 - 31):
 *   30 bit value counter of the number of times the group was entered.
 *   dispatch_group_enter counts downward on 32bits, and dispatch_group_leave
 *   upward on 64bits, which causes the generation to bump each time the value
 *   reaches 0 again due to carry propagation.
 * 进入组的次数的 30 位值计数器。 
 * dispatch_group_enter 在 32bits 上向下计数，dispatch_group_leave 在 64bits 上向上计数，
 * 这会导致每次由于进位传播，值再次达到 0 时产生碰撞。
 *
 * Has Notifs (1):
 *   This bit is set when the list of notifications on the group becomes non
 *   empty. It is also used as a lock as the thread that successfuly clears this
 *   bit is the thread responsible for firing the notifications.
 * 当组上的通知列表变为非空时设置此位。 它也用作锁，因为成功清除该位的线程是负责触发通知的线程。
 *
 * Has Waiters (0):
 *   This bit is set when there are waiters (threads in dispatch_group_wait)
 *   that need to be woken up the next time the value reaches 0. Waiters take
 *   a snapshot of the generation before waiting and will wait for the
 *   generation to change before they return.
 * 当有等待者（dispatch_group_wait 中的线程）需要在下一次值达到 0 时被唤醒时，设置该位。
 * 等待者在等待之前对世代进行快照，并在返回之前等待世代改变。
 */
#define DISPATCH_GROUP_GEN_MASK         0xffffffff00000000ULL
#define DISPATCH_GROUP_VALUE_MASK       0x00000000fffffffcULL
#define DISPATCH_GROUP_VALUE_INTERVAL   0x0000000000000004ULL
#define DISPATCH_GROUP_VALUE_1          DISPATCH_GROUP_VALUE_MASK
#define DISPATCH_GROUP_VALUE_MAX        DISPATCH_GROUP_VALUE_INTERVAL
#define DISPATCH_GROUP_HAS_NOTIFS       0x0000000000000002ULL
#define DISPATCH_GROUP_HAS_WAITERS      0x0000000000000001ULL
DISPATCH_CLASS_DECL(group, OBJECT);
struct dispatch_group_s {
	DISPATCH_OBJECT_HEADER(group);
	DISPATCH_UNION_LE(uint64_t volatile dg_state,
			uint32_t dg_bits,
			uint32_t dg_gen
	) DISPATCH_ATOMIC64_ALIGN;
	struct dispatch_continuation_s *volatile dg_notify_head;
	struct dispatch_continuation_s *volatile dg_notify_tail;
};


/* dispatch_group_s 结构体全部展开后
struct dispatch_group_extra_vtable_s {
    unsigned long const do_type;
    void (*const do_dispose)(struct dispatch_group_s *, bool *allow_free);
    size_t (*const do_debug)(struct dispatch_group_s *, char *, size_t);
    void (*const do_invoke)(struct dispatch_group_s *, dispatch_invoke_context_t, dispatch_invoke_flags_t);
};

struct dispatch_group_vtable_s {
    // _OS_OBJECT_CLASS_HEADER();
    void (*_os_obj_xref_dispose)(_os_object_t);
    void (*_os_obj_dispose)(_os_object_t);
    
    struct dispatch_group_extra_vtable_s _os_obj_vtable;
};

// OS_OBJECT_CLASS_SYMBOL(dispatch_group)

extern const struct dispatch_group_vtable_s _OS_dispatch_group_vtable;
extern const struct dispatch_group_vtable_s OS_dispatch_group_class __asm__("__" OS_STRINGIFY(dispatch_group) "_vtable");

struct dispatch_group_s {
    struct dispatch_object_s _as_do[0];
    struct _os_object_s _as_os_obj[0];
    
    // must be pointer-sized 
    const struct dispatch_group_vtable_s *do_vtable; 
    
    int volatile do_ref_cnt;
    int volatile do_xref_cnt;
    
    struct dispatch_group_s *volatile do_next;
    struct dispatch_queue_s *do_targetq;
    void *do_ctxt;
    void *do_finalizer;
    
    // 可看到上半部分和其它 GCD 对象都是相同的，毕竟大家都是继承自 dispatch_object_s，重点是下面的新内容
    
    union { 
        uint64_t volatile dg_state;  // leave 时加 DISPATCH_GROUP_VALUE_INTERVAL
        struct { 
            uint32_t dg_bits; // enter 时减 DISPATCH_GROUP_VALUE_INTERVAL
            
            // 主要用于 dispatch_group_wait 函数被调用后，
            // 当 dispath_group 处于 wait 状态时，结束等待的条件有两条：
            // 1): 当 dispatch_group 关联的 block 都执行完毕后，wait 状态结束
            // 2): 当到达了指定的等待时间后，即使关联的 block 没有执行完成，也结束 wait 状态 
            
            // 而当 dg_gen 不为 0 时，说明 dg_state 发生了进位，可表示 dispatch_group 关联的 block 都执行完毕了，
            // 如果 dispatch_group 此时处于 wait 状态的话就可以结束了，此时正对应上面结束 wait 状态的条件 1 中。
            uint32_t dg_gen;
        };
    } __attribute__((aligned(8)));
    
    // 下面两个成员变量比较特殊，它们分别是一个链表的头节点指针和尾节点指针
    // 调用 dispatch_group_notify 函数可添加当 dispatch_group 关联的 block 异步执行完成后的回调通知，
    // 多次调用 dispatch_group_notify 函数可添加多个回调事件（我们日常开发一般就用了一个回调事件，可能会忽略这个细节），
    // 而这些多个回调事件则会构成一个 dispatch_continuation_s 作为节点的链表，当 dispatch_group 中关联的 block 全部执行完成后，
    // 此链表中的 dispatch_continuation_s 都会得到异步执行。
    //（注意是异步，具体在哪个队列则根据 dispatch_group_notify 函数的入参决定，以及执行的优先级则根据队列的优先级决定）。
    
    struct dispatch_continuation_s *volatile dg_notify_head; // dispatch_continuation_s 链表的头部节点
    struct dispatch_continuation_s *volatile dg_notify_tail; // dispatch_continuation_s 链表的尾部节点
};
*/

DISPATCH_ALWAYS_INLINE
static inline uint32_t
_dg_state_value(uint64_t dg_state)
{
	return (uint32_t)(-((uint32_t)dg_state & DISPATCH_GROUP_VALUE_MASK)) >> 2;
}

DISPATCH_ALWAYS_INLINE
static inline uint32_t
_dg_state_gen(uint64_t dg_state)
{
	return (uint32_t)(dg_state >> 32);
}

dispatch_group_t _dispatch_group_create_and_enter(void);
void _dispatch_group_dispose(dispatch_object_t dou, bool *allow_free);
DISPATCH_COLD
size_t _dispatch_group_debug(dispatch_object_t dou, char *buf,
		size_t bufsiz);

void _dispatch_semaphore_dispose(dispatch_object_t dou, bool *allow_free);
DISPATCH_COLD
size_t _dispatch_semaphore_debug(dispatch_object_t dou, char *buf,
		size_t bufsiz);

#endif
