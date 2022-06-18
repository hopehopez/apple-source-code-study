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

#undef dispatch_once
#undef dispatch_once_f


#ifdef __BLOCKS__
void
dispatch_once(dispatch_once_t *val, dispatch_block_t block)
{
	dispatch_once_f(val, block, _dispatch_Block_invoke(block));
}
#endif

#if DISPATCH_ONCE_INLINE_FASTPATH
#define DISPATCH_ONCE_SLOW_INLINE inline DISPATCH_ALWAYS_INLINE
#else
#define DISPATCH_ONCE_SLOW_INLINE DISPATCH_NOINLINE
#endif // DISPATCH_ONCE_INLINE_FASTPATH

DISPATCH_NOINLINE
static void
_dispatch_once_callout(dispatch_once_gate_t l, void *ctxt,
		dispatch_function_t func)
{
	//执行一次block
	_dispatch_client_callout(ctxt, func);

	//将l赋值为1， 并唤醒被阻塞的线程（如果有的话）
	_dispatch_once_gate_broadcast(l);
}

DISPATCH_NOINLINE
void
dispatch_once_f(dispatch_once_t *val, void *ctxt, dispatch_function_t func)
{
	// 把 val 转换为 dispatch_once_gate_t 类型
	dispatch_once_gate_t l = (dispatch_once_gate_t)val;

#if !DISPATCH_ONCE_INLINE_FASTPATH || DISPATCH_ONCE_USE_QUIESCENT_COUNTER
 	// 原子性获取 l->dgo_once 的值
	uintptr_t v = os_atomic_load(&l->dgo_once, acquire);
    // 判断 v 的值是否是 DLOCK_ONCE_DONE（大概率是，表示 val 已经被赋值 DLOCK_ONCE_DONE 和 func 已经执行过了），是则直接返回
	if (likely(v == DLOCK_ONCE_DONE)) {
		return;
	}
#if DISPATCH_ONCE_USE_QUIESCENT_COUNTER
	// 判断 v 是否还存在锁，如果存在就返回
	if (likely(DISPATCH_ONCE_IS_GEN(v))) {
		return _dispatch_once_mark_done_if_quiesced(l, v);
	}
#endif
#endif
	//如果l是0时，会返回yes
	//返回yes代表block还未执行过，调用_dispatch_once_callout执行block
	//此时的l在_dispatch_once_gate_tryenter已经被赋值为线程id了
	if (_dispatch_once_gate_tryenter(l)) {
		//执行一次block, 并将l赋值为 DLOCK_ONCE_DONE
		return _dispatch_once_callout(l, ctxt, func);
	}
	//不停查询 &dgo->dgo_once 的值，若变为 DLOCK_ONCE_DONE 则调用 os_atomic_rmw_loop_give_up(return) 退出等待
	return _dispatch_once_wait(l);
}
