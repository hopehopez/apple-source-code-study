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

#include "internal.h"

DISPATCH_WEAK // rdar://problem/8503746
intptr_t _dispatch_semaphore_signal_slow(dispatch_semaphore_t dsema);

#pragma mark -
#pragma mark dispatch_semaphore_t

dispatch_semaphore_t
dispatch_semaphore_create(intptr_t value)
{
	// 指向 dispatch_semaphore_s 结构体的指针
	dispatch_semaphore_t dsema;

	// If the internal value is negative, then the absolute of the value is
	// equal to the number of waiting threads. Therefore it is bogus to
	// initialize the semaphore with a negative value.
	// 如果内部值为负，则该值的绝对值等于等待线程的数量。 因此，用负值初始化信号量是虚假的。
	if (value < 0) {
		//#define DISPATCH_BAD_INPUT		((void *_Nonnull)0)
		//如果初始值为负数，则直接返回0
		return DISPATCH_BAD_INPUT;
	}

	// DISPATCH_VTABLE(semaphore) ➡️ &OS_dispatch_semaphore_class
    // _dispatch_object_alloc 是为 dispatch_semaphore_s 申请空间，然后用 &OS_dispatch_semaphore_class 初始化，
    // &OS_dispatch_semaphore_class 设置了 dispatch_semaphore_t 的相关回调函数，如
	// 销毁函数 _dispatch_semaphore_dispose 等
	dsema = _dispatch_object_alloc(DISPATCH_VTABLE(semaphore),
			sizeof(struct dispatch_semaphore_s));

	//#define DISPATCH_OBJECT_LISTLESS ((void *)0xffffffff89abcdef)		
	dsema->do_next = DISPATCH_OBJECT_LISTLESS;

	//目标队列（从全局的队列数组 _dispatch_root_queues 中取默认队列， 下标为9）
	/*
	_DISPATCH_GLOBAL_ROOT_QUEUE_ENTRY(DEFAULT, DISPATCH_PRIORITY_FLAG_FALLBACK,
		.dq_label = "com.apple.root.default-qos",
		.dq_serialnum = 13,
	)
	*/
	dsema->do_targetq = _dispatch_get_default_queue(false);
	// 当前值（当前是初始值）
	dsema->dsema_value = value;

	//#define _DSEMA4_POLICY_FIFO SYNC_POLICY_FIFO 
	//同步器的等待顺序 先进先出
	_dispatch_sema4_init(&dsema->dsema_sema, _DSEMA4_POLICY_FIFO);
	dsema->dsema_orig = value;

	//初始值
	return dsema;
}

void
_dispatch_semaphore_dispose(dispatch_object_t dou,
		DISPATCH_UNUSED bool *allow_free)
{
	dispatch_semaphore_t dsema = dou._dsema;

	// 容错判断，如果当前 dsema_value 小于 dsema_orig，表示信号量还正在使用，不能进行销毁，如下代码会导致此 crash 
    // dispatch_semaphore_t sema = dispatch_semaphore_create(1); // 创建 value = 1，orig = 1
    // dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER); // value = 0，orig = 1
    // sema = dispatch_semaphore_create(1); // 赋值导致原始 dispatch_semaphore_s 释放，但是此时 orig 是 1，value 是 0 则直接 crash

	if (dsema->dsema_value < dsema->dsema_orig) {
		DISPATCH_CLIENT_CRASH(dsema->dsema_orig - dsema->dsema_value,
				"Semaphore object deallocated while in use");
	}

	_dispatch_sema4_dispose(&dsema->dsema_sema, _DSEMA4_POLICY_FIFO);
}

size_t
_dispatch_semaphore_debug(dispatch_object_t dou, char *buf, size_t bufsiz)
{
	dispatch_semaphore_t dsema = dou._dsema;

	size_t offset = 0;
	offset += dsnprintf(&buf[offset], bufsiz - offset, "%s[%p] = { ",
			_dispatch_object_class_name(dsema), dsema);
	offset += _dispatch_object_debug_attr(dsema, &buf[offset], bufsiz - offset);
#if USE_MACH_SEM
	offset += dsnprintf(&buf[offset], bufsiz - offset, "port = 0x%x, ",
			dsema->dsema_sema);
#endif
	offset += dsnprintf(&buf[offset], bufsiz - offset,
			"value = %" PRIdPTR ", orig = %" PRIdPTR " }", dsema->dsema_value, dsema->dsema_orig);
	return offset;
}

DISPATCH_NOINLINE
intptr_t
_dispatch_semaphore_signal_slow(dispatch_semaphore_t dsema)
{
	//判断dsema->dsema_sema是否已赋值，没有则赋值为_DSEMA4_POLICY_FIFO
	_dispatch_sema4_create(&dsema->dsema_sema, _DSEMA4_POLICY_FIFO);
	// count 传 1，唤醒一条线程
	_dispatch_sema4_signal(&dsema->dsema_sema, 1);
	return 1;
}

// dispatch_semaphore_signal 发信号（增加）信号量。
// 如果先前的值小于零，则此函数在返回之前唤醒等待的线程。如果线程被唤醒，此函数将返回非零值。否则，返回零。
intptr_t
dispatch_semaphore_signal(dispatch_semaphore_t dsema)
{
	// 原子操作 dsema 的成员变量 dsema_value 的值加 1
	long value = os_atomic_inc2o(dsema, dsema_value, release);
	if (likely(value > 0)) {
		// 如果 value 大于 0 表示目前没有线程需要唤醒，直接 return 0
		return 0;
	}
	// 如果过度释放，导致 value 的值一直增加到 LONG_MIN（溢出），则 crash 
	if (unlikely(value == LONG_MIN)) {
		DISPATCH_CLIENT_CRASH(value,
				"Unbalanced call to dispatch_semaphore_signal()");
	}
	// value 小于等于 0 时，表示目前有线程需要唤醒
	return _dispatch_semaphore_signal_slow(dsema);
}

DISPATCH_NOINLINE
static intptr_t
_dispatch_semaphore_wait_slow(dispatch_semaphore_t dsema,
		dispatch_time_t timeout)
{
	long orig;
	//检查dsema->dsema_sema是否赋值，如果没有则为它赋值_DSEMA4_POLICY_FIFO
	_dispatch_sema4_create(&dsema->dsema_sema, _DSEMA4_POLICY_FIFO);

	switch (timeout) {
	
	// 当 timeout 是一个指定的时间的话，则循环等待直到超时，或者发出了 signal 信号，sema 值被修改。
	default:
		if (!_dispatch_sema4_timedwait(&dsema->dsema_sema, timeout)) {
			break;
		}
		// Try to undo what the fast path did to dsema->dsema_value
		DISPATCH_FALLTHROUGH;


	// 如果 timeout 参数是 DISPATCH_TIME_NOW
	case DISPATCH_TIME_NOW:
		orig = dsema->dsema_value;
		while (orig < 0) {
			// dsema_value 加 1 抵消掉 dispatch_semaphore_wait 函数中的减 1 操作
			if (os_atomic_cmpxchgv2o(dsema, dsema_value, orig, orig + 1,
					&orig, relaxed)) {
						// 返回超时
				return _DSEMA4_TIMEOUT();
			}
		}
		// Another thread called semaphore_signal(). Drain the wakeup.
		DISPATCH_FALLTHROUGH;
	
	// 如果 timeout 参数是 DISPATCH_TIME_FOREVER 的话调用 _dispatch_sema4_wait 一直等待，直到得到 signal 信号
	case DISPATCH_TIME_FOREVER:
	//_dispatch_sema4_wait内是一个do-while循环，直到dsema_sema==KERN_ABORTED时，循环结束
		_dispatch_sema4_wait(&dsema->dsema_sema);
		break;
	}
	return 0;
}

intptr_t
dispatch_semaphore_wait(dispatch_semaphore_t dsema, dispatch_time_t timeout)
{
	// 原子操作 dsema 的成员变量 dsema_value 的值减 1
	long value = os_atomic_dec2o(dsema, dsema_value, acquire);

	// 如果减 1 后仍然大于等于 0，则直接 return 
	if (likely(value >= 0)) {
		return 0;
	}

	// 如果小于 0，则调用 _dispatch_semaphore_wait_slow 函数进行阻塞等待
	return _dispatch_semaphore_wait_slow(dsema, timeout);
}

#pragma mark -
#pragma mark dispatch_group_t

DISPATCH_ALWAYS_INLINE
static inline dispatch_group_t
_dispatch_group_create_with_count(uint32_t n)
{	
	// DISPATCH_VTABLE(group) 宏定义展开是 ➡️：&OS_dispatch_group_class
    
    // _dispatch_object_alloc 是为 dispatch_group_s 申请空间，然后用 &OS_dispatch_group_class 初始化，
    // &OS_dispatch_group_class 设置了 dispatch_group_t 的相关回调函数，如销毁函数 _dispatch_group_dispose 等。
	dispatch_group_t dg = _dispatch_object_alloc(DISPATCH_VTABLE(group),
			sizeof(struct dispatch_group_s));
	// 表示链表的下一个节点，（目前赋一个初值 DISPATCH_OBJECT_LISTLESS）
	dg->do_next = DISPATCH_OBJECT_LISTLESS;
	// 目标队列（从全局的根队列数组 _dispatch_root_queues 中取默认 QOS 的队列）
	dg->do_targetq = _dispatch_get_default_queue(false);
	if (n) {
		// ⬇️ 以原子方式把 (uint32_t)-n * DISPATCH_GROUP_VALUE_INTERVAL 的值存储到 dg_bits 中，
        // n 表示 dg 关联的 block 数量。
		// #define DISPATCH_GROUP_VALUE_INTERVAL   0x0000000000000004ULL
		// 将-4转为uint32_t 后是  4294967292  16进制:0xFFFFFFFC 2进制：1111111...100， 这样不会对低位的两位造成影响
		os_atomic_store2o(dg, dg_bits,
				(uint32_t)-n * DISPATCH_GROUP_VALUE_INTERVAL, relaxed);
		// ⬇️ 以原子方式把 1 保存到 do_ref_cnt 中（表示 dispatch_group 内部引用计数为 1，即目前有与组关联的 block 或者有任务进组了）
		os_atomic_store2o(dg, do_ref_cnt, 1, relaxed); // <rdar://22318411>
	}
	return dg;
}

dispatch_group_t
dispatch_group_create(void)
{
	return _dispatch_group_create_with_count(0);
}

dispatch_group_t
_dispatch_group_create_and_enter(void)
{
	return _dispatch_group_create_with_count(1);
}

void
_dispatch_group_dispose(dispatch_object_t dou, DISPATCH_UNUSED bool *allow_free)
{
	uint64_t dg_state = os_atomic_load2o(dou._dg, dg_state, relaxed);

	if (unlikely((uint32_t)dg_state)) {
		DISPATCH_CLIENT_CRASH((uintptr_t)dg_state,
				"Group object deallocated while in use");
	}
}

size_t
_dispatch_group_debug(dispatch_object_t dou, char *buf, size_t bufsiz)
{
	dispatch_group_t dg = dou._dg;
	uint64_t dg_state = os_atomic_load2o(dg, dg_state, relaxed);

	size_t offset = 0;
	offset += dsnprintf(&buf[offset], bufsiz - offset, "%s[%p] = { ",
			_dispatch_object_class_name(dg), dg);
	offset += _dispatch_object_debug_attr(dg, &buf[offset], bufsiz - offset);
	offset += dsnprintf(&buf[offset], bufsiz - offset,
			"count = %u, gen = %d, waiters = %d, notifs = %d }",
			_dg_state_value(dg_state), _dg_state_gen(dg_state),
			(bool)(dg_state & DISPATCH_GROUP_HAS_WAITERS),
			(bool)(dg_state & DISPATCH_GROUP_HAS_NOTIFS));
	return offset;
}

DISPATCH_NOINLINE
static intptr_t
_dispatch_group_wait_slow(dispatch_group_t dg, uint32_t gen,
		dispatch_time_t timeout)
{
	 // for 死循环，等待内部的条件满足时 return，否则一直进行死循环
	for (;;) {
		// 比较等待，内部是根据指定的时间进行时间等待，并根据 &dg->dg_gen 值判断是否关联的 block 都异步执行完毕了。
        // 这里牵涉到 dg_state 的进位，当 dg_bits 溢出时会进位到 dg_gen 中，此时 dg_gen 不再是 0，
		// 可表示关联的 block 都执行完毕了。
		int rc = _dispatch_wait_on_address(&dg->dg_gen, gen, timeout, 0);

		// 表示 dispatch_group 关联的 block 都异步执行完毕了，return 0
		if (likely(gen != os_atomic_load2o(dg, dg_gen, acquire))) {
			return 0;
		}

		// 等到超过指定时间了，return _DSEMA4_TIMEOUT() 超时
		if (rc == ETIMEDOUT) {
			return _DSEMA4_TIMEOUT();
		}
	}
}


/*
dispatch_group_wait 函数同步等待直到与 dispatch_group 关联的所有 block 都异步执行完成或者直到指定的超时时间过去为止，
才会返回。
 如果没有与 dispatch_group 关联的 block，则此函数将立即返回。
 从多个线程同时使用同一 dispatch_group 调用此函数的结果是不确定的。
 成功返回此函数后，dispatch_group 关联的 block 为空，可以使用 dispatch_release 释放 dispatch_group，
也可以将其重新用于其它 block。
*/
intptr_t
dispatch_group_wait(dispatch_group_t dg, dispatch_time_t timeout)
{
	uint64_t old_state, new_state;

	// 使用同上面的 os_atomic_rmw_loop2o 宏定义，内部是一个 do while 循环，
    // 每次循环都从本地原子取值，判断 dispatch_group 所处的状态，
    // 是否关联的 block 都异步执行完毕了。 
	// 原子取出dg_state的值存入old_state
	os_atomic_rmw_loop2o(dg, dg_state, old_state, new_state, relaxed, {
		// #define DISPATCH_GROUP_VALUE_MASK   0x00000000fffffffcULL

		if ((old_state & DISPATCH_GROUP_VALUE_MASK) == 0) {
			 // 表示关联的 block 为 0 或者关联的 block 都执行完毕了，则直接 return 0，
       		 //（函数返回，停止阻塞当前线程。）
			// 跳出循环并返回 0
			os_atomic_rmw_loop_give_up_with_fence(acquire, return 0);
		}

		// 如果 timeout 等于 0，则立即跳出循环并返回 _DSEMA4_TIMEOUT()，
        // 指定等待时间为 0，则函数返回，并返回超时提示，
        //（继续向下执行，停止阻塞当前线程。）
		if (unlikely(timeout == 0)) {
			// 跳出循环并返回 _DSEMA4_TIMEOUT() 超时
			os_atomic_rmw_loop_give_up(return _DSEMA4_TIMEOUT());
		}

		// #define DISPATCH_GROUP_HAS_WAITERS   0x0000000000000001ULL
		new_state = old_state | DISPATCH_GROUP_HAS_WAITERS;

		// 表示目前需要等待，至少等到关联的 block 都执行完毕或者等到指定时间超时 
		if (unlikely(old_state & DISPATCH_GROUP_HAS_WAITERS)) {
			// 跳出循环，执行下面的 _dispatch_group_wait_slow 函数
			os_atomic_rmw_loop_give_up(break);
		}
	});

	// ({ 
	// 	bool _result = false; 
	// 	__typeof__(&(dg)->dg_state) _p = (&(dg)->dg_state); 
	// 	old_state = os_atomic_load(_p, relaxed); 
	// 	do { 
	// 		 { 
	// 			if ((old_state & 0x00000000fffffffcULL) == 0) { 
	// 				({ 
	// 					__c11_atomic_thread_fence(memory_order_acquire); 
	// 					return 0; 
	// 					__builtin_unreachable(); 
	// 				}); 
	// 			} 
	// 			if (__builtin_expect(!!(timeout == 0), 0)) { 
	// 				({ 
	// 					__c11_atomic_thread_fence(memory_order_relaxed); 
	// 					return 49; __builtin_unreachable(); 
	// 				}); 
	// 			} 
	// 			new_state = old_state | 0x0000000000000001ULL; 
	// 			if (__builtin_expect(!!(old_state & 0x0000000000000001ULL), 0)) { 
	// 				({
	// 					 __c11_atomic_thread_fence(memory_order_relaxed); 
	// 					 break; 
	// 					 __builtin_unreachable(); 
	// 				}); 
	// 			} 
	// 		}; 
					
	// 		_result = os_atomic_cmpxchgvw(_p, old_state, new_state, &old_state, relaxed);
			
	// 	} while (unlikely(!_result)); 
	// 	_result; 
	// })

	return _dispatch_group_wait_slow(dg, _dg_state_gen(new_state), timeout);
}


/*
_dispatch_group_wake 把  notify 回调函数链表中的所有的函数提交到指定的队列中异步执行，
needs_release 表示是否需要释放所有关联 block 异步执行完成、所有的 notify 回调函数执行完成的 dispatch_group 对象。
dg_state 则是 dispatch_group 的状态，包含目前的关联的 block 数量等信息。
*/
DISPATCH_NOINLINE
static void
_dispatch_group_wake(dispatch_group_t dg, uint64_t dg_state, bool needs_release)
{
	// dispatch_group 对象的引用计数是否需要 -1
	uint16_t refs = needs_release ? 1 : 0; // <rdar://problem/22318411>

	// #define DISPATCH_GROUP_HAS_NOTIFS   0x0000000000000002ULL // 用来判断 dispatch_group 是否存在 notify 函数的掩码
	// 这里如果 dg_state & 0x0000000000000002ULL 结果不为 0，即表示 dg 存在 notify 回调函数
	if (dg_state & DISPATCH_GROUP_HAS_NOTIFS) {
		dispatch_continuation_t dc, next_dc, tail;

		// Snapshot before anything is notified/woken <rdar://problem/8554546>
		// 取出 dg 的 notify 回调函数链表的头 
		dc = os_mpsc_capture_snapshot(os_mpsc(dg, dg_notify), &tail);
		do {
			// 取出 dc 创建时指定的队列，对应 _dispatch_group_notify 函数中的 dsn->dc_data = dq 赋值操作
			dispatch_queue_t dsn_queue = (dispatch_queue_t)dc->dc_data;

			// 取得下一个节点
			next_dc = os_mpsc_pop_snapshot_head(dc, tail, do_next);

			// 根据各队列的优先级异步执行 notify 链表中的函数
			_dispatch_continuation_async(dsn_queue, dc,
					_dispatch_qos_from_pp(dc->dc_priority), dc->dc_flags);
			
			// 释放 notify 函数执行时的队列 dsn_queue（os_obj_ref_cnt - 1）, 对应之前的计数+1
			_dispatch_release(dsn_queue);

			// 当 next_dc 为 NULL 时，跳出循环
		} while ((dc = next_dc));

        // 这里的 refs 计数增加 1 正对应了 _dispatch_group_notify 函数中，
        // 当第一次给 dispatch_group 添加 notify 函数时的引用计数加 1，_dispatch_retain(dg)
        // 代码执行到这里时 dg 的所有 notify 函数都执行完毕了。 
        //（统计 dispatch_group 的引用计数需要减小的值）
		refs++;
	}

 	// #define DISPATCH_GROUP_HAS_WAITERS   0x0000000000000001ULL
	// 根据 &dg->dg_gen 的值判断是否处于阻塞状态
	if (dg_state & DISPATCH_GROUP_HAS_WAITERS) {
		_dispatch_wake_by_address(&dg->dg_gen);
	}

	// 根据 refs 判断是否需要释放 dg（执行 os_obj_ref_cnt - refs），当 os_obj_ref_cnt 的值小于 0 时，可销毁 dg。
    // 如果 needs_release 为真，并且 dg 有 notify 函数时，会执行 os_obj_ref_cnt - 2
    // 如果 needs_release 为假，但是 dg 有 notify 函数时，会执行 os_obj_ref_cnt - 1
    // 如果 needs_release 为假，且 dg 无 notify 函数时，不执行操作
	if (refs) _dispatch_release_n(dg, refs);
}

///dispatch_group_leave 手动指示 dispatch_group 中的一个关联 block 已完成，或者说是一个 block 已解除关联。
///调用此函数表示一个关联 block 已完成，并且已通过 dispatch_group_async 以外的方式与 dispatch_group 解除了关联。
void
dispatch_group_leave(dispatch_group_t dg)
{
	// The value is incremented on a 64bits wide atomic so that the carry for
	// the -1 -> 0 transition increments the generation atomically.
	// 以原子方式增加 dg_state 的值，dg_bits 的内存空间是 dg_state 的低 32 bit，
    // 所以 dg_state + DISPATCH_GROUP_VALUE_INTERVAL 没有进位到 33 bit 时都可以理解为是 dg_bits + DISPATCH_GROUP_VALUE_INTERVAL。

	//（这里注意是把 dg_state 的旧值同时赋值给了 new_state 和 old_state 两个变量）
	uint64_t new_state, old_state = os_atomic_add_orig2o(dg, dg_state,
			DISPATCH_GROUP_VALUE_INTERVAL, release);

	// #define DISPATCH_GROUP_VALUE_MASK   0x00000000fffffffcULL ➡️ 0b0000...11111100ULL
    // #define DISPATCH_GROUP_VALUE_1   DISPATCH_GROUP_VALUE_MASK
    
    // dg_state 的旧值和 DISPATCH_GROUP_VALUE_MASK 进行与操作掩码取值，消除dg_state低2位的影响
	// 如果此时仅关联了一个 block 的话那么 dg_state 的旧值就是（十六进制：0xFFFFFFFC），
    //（那么上面的 os_atomic_add_orig2o 执行后，dg_state 的值是 0x0000000100000000ULL，
    // 因为它是 uint64_t 类型它会从最大的 uint32_t 继续进位，而不同于 dg_bits 的 uint32_t 类型溢出后为 0）
	// 注意 此时强转为uint32
	uint32_t old_value = (uint32_t)(old_state & DISPATCH_GROUP_VALUE_MASK);

	if (unlikely(old_value == DISPATCH_GROUP_VALUE_1)) {
		//如果old_value == 0xFFFFFFFC，说明此时的如果dg_state = 0x0000000100000000ULL已经产生进位了
		//此时dg_state的高32位为0x00000001, dg_gen也是1 由于联合体的存在，其实它俩是同一个

		// old_state 是 0x00000000fffffffcULL，DISPATCH_GROUP_VALUE_INTERVAL 的值是 0x0000000000000004ULL
        // 由于这里 old_state 是 uint64_t 类型，加 DISPATCH_GROUP_VALUE_INTERVAL 后不会发生溢出会产生正常的进位，
		// old_state = 0x00000001000000xxULL
		old_state += DISPATCH_GROUP_VALUE_INTERVAL;
		do {
		
			// new_state = 0x00000001000000xx
			new_state = old_state;
			if ((old_state & DISPATCH_GROUP_VALUE_MASK) == 0) {
				// 如果目前是仅关联了一个 block 而且是正常的 enter 和 leave 配对执行，则会执行这里。

				//#define DISPATCH_GROUP_HAS_NOTIFS       0x0000000000000002ULL
				//#define DISPATCH_GROUP_HAS_WAITERS      0x0000000000000001ULL

				// 清理 new_state 中对应 DISPATCH_GROUP_HAS_WAITERS 的非零位的值，
                // 即把 new_state 二进制表示的倒数第一位置 0
				new_state &= ~DISPATCH_GROUP_HAS_WAITERS; 

				// 清理 new_state 中对应 DISPATCH_GROUP_HAS_NOTIFS 的非零位的值，
                // 即把 new_state 二进制表示的倒数第二位置 0
				new_state &= ~DISPATCH_GROUP_HAS_NOTIFS;
			} else {
				// If the group was entered again since the atomic_add above,
				// we can't clear the waiters bit anymore as we don't know for
				// which generation the waiters are for

				new_state &= ~DISPATCH_GROUP_HAS_NOTIFS;
			}
			// 如果目前是仅关联了一个 block 而且是正常的 enter 和 leave 配对执行，则会执行这里的 break，
            // 结束 do while 循环，执行下面的 _dispatch_group_wake 函数，
			// 唤醒异步执行 dispatch_group_notify 添加到指定队列中的回调通知。
			if (old_state == new_state) break;


		// 比较 dg_state 和 old_state 的值，如果相等则把 dg_state 的值存入 new_state 中，并返回 true，
		//如果不相等则把 dg_state 的值存入 old_state 中，并返回 false。
        // unlikely(!os_atomic_cmpxchgv2o(dg, dg_state, old_state, new_state, &old_state, relaxed)) 
		//表达式值为 false 时才会结束循环，否则继续循环，
        // 即 os_atomic_cmpxchgv2o(dg, dg_state, old_state, new_state, &old_state, relaxed) 
		//返回 true 时才会结束循环，否则继续循环，
        // 即 dg_state 和 old_state 的值相等时才会结束循环，否则继续循环。
        
        //（正常 enter 和 leave 的话，此时 dg_state 和 old_state 的值都是 0x0000000100000000ULL，会结束循环）
		} while (unlikely(!os_atomic_cmpxchgv2o(dg, dg_state,
				old_state, new_state, &old_state, relaxed)));

		// 唤醒异步执行 dispatch_group_notify 添加到指定队列中的回调通知		
		return _dispatch_group_wake(dg, old_state, true);
	}

    // 如果 old_value 为 0，而上面又进行了一个 dg_state + DISPATCH_GROUP_VALUE_INTERVAL 操作，
	// 此时就过度 leave 了，则 crash，
    // 例如创建好一个 dispatch_group 后直接调用 dispatch_group_leave 函数即会触发这个 crash。
	if (unlikely(old_value == 0)) {
		DISPATCH_CLIENT_CRASH((uintptr_t)old_value,
				"Unbalanced call to dispatch_group_leave()");
	}
}

///dispatch_group_enter 表示 dispatch_group 已手动关联一个 block。
///调用此函数表示一个 block 已通过 dispatch_group_async 以外的方式与 dispatch_group 建立关联，
///对该函数的调用必须与 dispatch_group_leave 保持平衡。
void
dispatch_group_enter(dispatch_group_t dg)
{
	// The value is decremented on a 32bits wide atomic so that the carry
	// for the 0 -> -1 transition is not propagated to the upper 32bits.
	// dg_bits 是无符号 32 位 int，-1 和 0 的转换在 32 位 int 范围内，不会过渡到高位，影响 dg_gen 和 dg_state 的值

	// #define DISPATCH_GROUP_VALUE_INTERVAL   0x0000000000000004ULL
	// 每次有block与group建立关联时，就在原有计数的基础上-4
	uint32_t old_bits = os_atomic_sub_orig2o(dg, dg_bits,
			DISPATCH_GROUP_VALUE_INTERVAL, acquire);

	// #define DISPATCH_GROUP_VALUE_MASK   0x00000000fffffffcULL 二进制表示 ➡️ 0b0000...11111100ULL
    // 拿 old_bits 和 DISPATCH_GROUP_VALUE_MASK 进行与操作，取出 dg_bits 的旧值，
    // old_bits 的二进制表示的后两位是其它作用的掩码标记位，需要做这个与操作把它们置为 0，
    // old_value 可用来判断这次 enter 之前 dispatch_group 内部是否关联过 block。
	uint32_t old_value = old_bits & DISPATCH_GROUP_VALUE_MASK;
	if (unlikely(old_value == 0)) {
		// 表示此时调度组由未关联任何 block 的状态变换到了关联了一个 block 的状态，
        // 调用 _dispatch_retain 把 dg 的内部引用计数 +1 表明 dg 目前正在被使用，不能进行销毁。
		_dispatch_retain(dg); // <rdar://problem/22318411>
	}

	// #define DISPATCH_GROUP_VALUE_INTERVAL   0x0000000000000004ULL 二进制表示 ➡️ 0b0000...00000100ULL 
    // #define DISPATCH_GROUP_VALUE_MAX   DISPATCH_GROUP_VALUE_INTERVAL
    
    // 如果 old_bits & DISPATCH_GROUP_VALUE_MASK 的结果等于 DISPATCH_GROUP_VALUE_MAX，
	// 即 old_bits 的值是 DISPATCH_GROUP_VALUE_INTERVAL。
    // 这里可以理解为上面 4294967292 每次减 4，一直往下减，直到溢出...
    // 表示 dispatch_group_enter 函数过度调用，则 crash。
    // DISPATCH_GROUP_VALUE_MAX = 0 + DISPATCH_GROUP_VALUE_INTERVAL; 
	// 当old_value==DISPATCH_GROUP_VALUE_MAX==0x4时，再减4 就变成0了, 此时的0 跟 初始值是0已经不一样了，
	// 已经分不清当前与group关联的block数目是0 还是极大值了， 所以此时crash
	if (unlikely(old_value == DISPATCH_GROUP_VALUE_MAX)) {
		DISPATCH_CLIENT_CRASH(old_bits,
				"Too many nested calls to dispatch_group_enter()");
	}
}

DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_group_notify(dispatch_group_t dg, dispatch_queue_t dq,
		dispatch_continuation_t dsn)
{
	uint64_t old_state, new_state;
	dispatch_continuation_t prev;

	// dispatch_continuation_t 的 dc_data 成员变量被赋值为 dispatch_continuation_s 执行时所在的队列 
	dsn->dc_data = dq;

	// dq 队列引用计数 +1，因为有新的 dsn 要在这个 dq 中执行了（`os_obj_ref_cnt` 的值 +1）
	_dispatch_retain(dq);

	//    prev =  ({
    //        // 以下都是原子操作:
    //        _os_atomic_basetypeof(&(dg)->dg_notify_head) _tl = (dsn); // 类型转换
    //        // 把 dsn 的 do_next 置为 NULL，防止错误数据
    //        os_atomic_store(&(_tl)->do_next, (NULL), relaxed);
    //        // 入参 dsn 存储到 dg 的成员变量 dg_notify_tail 中，并返回之前的旧的 dg_notify_tail
    //        atomic_exchange_explicit(_os_atomic_c11_atomic(&(dg)->dg_notify_tail), _tl, memory_order_release);
    //    });
    
    // 把 dsn 存储到 dg 的 dg_notify_tail 成员变量中，并返回之前的旧 dg_notify_tail，
    // 这个 dg_notify_tail 是一个指针，用来指向 dg 的 notify 回调函数链表的尾节点。
	prev = os_mpsc_push_update_tail(os_mpsc(dg, dg_notify), dsn, do_next);

	// #define os_mpsc_push_was_empty(prev) ((prev) == NULL)

	// 如果 prev 为 NULL，表示 dg 是第一次添加 notify 回调函数，则再次增加 dg 的引用计数（os_obj_ref_cnt + 1），
    // 前面我们还看到 dg 在第一次执行 enter 时也会增加一次引用计数（os_obj_ref_cnt + 1）。
	if (os_mpsc_push_was_empty(prev)) _dispatch_retain(dg);

	//    ({
    //        // prev 是指向 notify 回调函数链表的尾节点的一个指针
    //        _os_atomic_basetypeof(&(dg)->dg_notify_head) _prev = (prev);
    //        if (likely(_prev)) {
    //            // 如果之前的尾节点存在，则把 dsn 存储到之前尾节点的 do_next 中，即进行了链表拼接
    //            (void)os_atomic_store(&(_prev)->do_next, ((dsn)), relaxed);
    //        } else {
    //            // 如果之前尾节点不存在，则表示链表为空，则 dsn 就是头节点了，并存储到 dg 的 dg_notify_head 成员变量中
    //            (void)os_atomic_store(&(dg)->dg_notify_head, (dsn), relaxed);
    //        }
    //    });
    
    // 把 dsn 拼接到 dg 的 notify 回调函数链表中，或者是第一次的话，则把 dsn 作为 notify 回调函数链表的头节点
	os_mpsc_push_update_prev(os_mpsc(dg, dg_notify), prev, dsn, do_next);


	if (os_mpsc_push_was_empty(prev)) {
		// 如果 prev为 NULL 的话，表示第一次添加notify, 开始一个do while 循环

		// os_atomic_rmw_loop2o 是一个宏定义，内部包裹了一个 do while 循环，
        // 直到 old_state == 0 时跳出循环执行 _dispatch_group_wake 函数唤醒执行 notify 链表中的回调通知，
        // 即对应我们上文中的 dispatch_group_leave 函数中 dg_bits 的值回到 0 表示 dispatch_group 中关联的 block 都执行完了。
        

        // 只要记得这里是用一个 do while 循环等待，每次循环以原子方式读取状态值（dg_bits），
        // 直到 0 状态，去执行 _dispatch_group_wake 唤醒函数把 notify 链表中的函数提交到指定的队列异步执行就好了！⛽️⛽️
		os_atomic_rmw_loop2o(dg, dg_state, old_state, new_state, release, {

			// #define DISPATCH_GROUP_HAS_NOTIFS   0x0000000000000002ULL
            
            // 这里挺重要的一个点，把 new_state 的二进制表示的倒数第二位置为 1，
            // 表示 dg 存在 notify 回调函数。
			new_state = old_state | DISPATCH_GROUP_HAS_NOTIFS;
			if ((uint32_t)old_state == 0) {
				os_atomic_rmw_loop_give_up({
					// 跳出循环执行 _dispatch_group_wake 函数，把 notify 回调函数链表中的任务提交到指定的队列中执行
					return _dispatch_group_wake(dg, new_state, false);
				});
			}
		});
	}
}


DISPATCH_NOINLINE
void
dispatch_group_notify_f(dispatch_group_t dg, dispatch_queue_t dq, void *ctxt,
		dispatch_function_t func)
{
	dispatch_continuation_t dsn = _dispatch_continuation_alloc();
	_dispatch_continuation_init_f(dsn, dq, ctxt, func, 0, DC_FLAG_CONSUME);
	_dispatch_group_notify(dg, dq, dsn);
}

#ifdef __BLOCKS__
/*
dispatch_group_notify 函数，当与 dispatch_group 相关联的所有 block 都已完成时，
计划将 db 提交到队列 dq（即当与 dispatch_group 相关联的所有 block 都已完成时，
notify 添加的回调通知将得到执行）。如果没有 block 与 dispatch_group 相关联，则通知块 db 将立即提交。
*/
void
dispatch_group_notify(dispatch_group_t dg, dispatch_queue_t dq,
		dispatch_block_t db)
{
	// 从缓存中取一个 dispatch_continuation_t 或者新建一个 dispatch_continuation_t 返回
	dispatch_continuation_t dsn = _dispatch_continuation_alloc();
	// 配置 dsn，即用 dispatch_continuation_s 封装 db。（db 转换为函数）
	_dispatch_continuation_init(dsn, dq, db, 0, DC_FLAG_CONSUME);
	// 调用 _dispatch_group_notify 函数
	_dispatch_group_notify(dg, dq, dsn);
}
#endif

/*
_dispatch_continuation_group_async 函数内部调用的函数很清晰，首先调用 enter 表示 block 与 dispatch_group 建立关联，
然后把 dispatch_group 赋值给 dispatch_continuation 的 dc_data 成员变量，
这里的用途是当执行完 dispatch_continuation 中的函数后从 dc_data 中读取到 dispatch_group，
然后对此 dispatch_group 进行一次出组 leave 操作（详情看下面的 _dispatch_continuation_with_group_invoke 函数），
正是和这里的 enter 操作平衡的，然后就是我们比较熟悉的 _dispatch_continuation_async 函数提交任务到队列中进行异步调用。
*/
DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_continuation_group_async(dispatch_group_t dg, dispatch_queue_t dq,
		dispatch_continuation_t dc, dispatch_qos_t qos)
{
	// 调用 dispatch_group_enter 表示与一个 block 建立关联, dg_bits-0x4
	dispatch_group_enter(dg);
	
	// 把 dg 赋值给了 dc 的 dc_data 成员变量，当 dc 中的函数执行完成后，
	//从 dc_data 中读出 dg 执行 leave 操作，正是和上面的 enter 操作对应。
	dc->dc_data = dg;

	// 在指定队列中异步执行 dc
	_dispatch_continuation_async(dq, dc, qos, dc->dc_flags);
}

/*
 dispatch_group_async 将一个 block 提交到指定的调度队列并进行异步调用，并将该 block 与给定的 dispatch_group 关联
（其内部自动插入了 dispatch_group_enter 和 dispatch_group_leave 操作，相当于 dispatch_async
 和 dispatch_group_enter、dispatch_group_leave 三个函数的一个封装）。
*/
DISPATCH_NOINLINE
void
dispatch_group_async_f(dispatch_group_t dg, dispatch_queue_t dq, void *ctxt,
		dispatch_function_t func)
{
	// 从缓存中取一个 dispatch_continuation_t 或者新建一个 dispatch_continuation_t 返回赋值给 dc。
	dispatch_continuation_t dc = _dispatch_continuation_alloc();
	// 这里的 DC_FLAG_GROUP_ASYNC 的标记很重要，是它标记了 dispatch_continuation 中的函数异步执行时具体调用哪个函数。
	uintptr_t dc_flags = DC_FLAG_CONSUME | DC_FLAG_GROUP_ASYNC;
	// 优先级
	dispatch_qos_t qos;
	// 配置 dsn，（db block 转换为函数）
	qos = _dispatch_continuation_init_f(dc, dq, ctxt, func, 0, dc_flags);
	// 调用 _dispatch_continuation_group_async 函数异步执行提交到 dq 的 db
	_dispatch_continuation_group_async(dg, dq, dc, qos);
}

#ifdef __BLOCKS__
///dispatch_group_async 将一个 block 提交到指定的调度队列并进行异步调用，并将该 block 与给定的 dispatch_group 关联
//（其内部自动插入了 dispatch_group_enter 和 dispatch_group_leave 操作，
//相当于 dispatch_async 和 dispatch_group_enter、dispatch_group_leave 三个函数的一个封装）
void
dispatch_group_async(dispatch_group_t dg, dispatch_queue_t dq,
		dispatch_block_t db)
{
	// 从缓存中取一个 dispatch_continuation_t 或者新建一个 dispatch_continuation_t 返回赋值给 dc。
	dispatch_continuation_t dc = _dispatch_continuation_alloc();

	//#define DC_FLAG_CONSUME					0x004ul
	//#define DC_FLAG_GROUP_ASYNC				0x008ul
	//dc_flags = 0xc
	uintptr_t dc_flags = DC_FLAG_CONSUME | DC_FLAG_GROUP_ASYNC;
	dispatch_qos_t qos;

	qos = _dispatch_continuation_init(dc, dq, db, 0, dc_flags);
	_dispatch_continuation_group_async(dg, dq, dc, qos);
}
#endif
