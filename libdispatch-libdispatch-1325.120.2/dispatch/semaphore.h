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

#ifndef __DISPATCH_SEMAPHORE__
#define __DISPATCH_SEMAPHORE__

#ifndef __DISPATCH_INDIRECT__
#error "Please #include <dispatch/dispatch.h> instead of this file directly."
#include <dispatch/base.h> // for HeaderDoc
#endif

DISPATCH_ASSUME_NONNULL_BEGIN

/*!
 * @typedef dispatch_semaphore_t
 *
 * @abstract
 * A counting semaphore.
 * 表示计数信号量。主要用来控制并发任务的数量。
 */
DISPATCH_DECL(dispatch_semaphore);

__BEGIN_DECLS

/*!
 * @function dispatch_semaphore_create
 *
 * @abstract
 * Creates new counting semaphore with an initial value.
 * 用初始值（long value）创建新的计数信号量。
 *
 * @discussion
 * Passing zero for the value is useful for when two threads need to reconcile
 * the completion of a particular event. Passing a value greater than zero is
 * useful for managing a finite pool of resources, where the pool size is equal
 * to the value.
 * 当两个线程需要协调特定事件的完成时，将值传递为零非常有用。传递大于零的值对于管理有限的资源池非常有用，该资源池的大小等于该值。
 *
 * @param value
 * The starting value for the semaphore. Passing a value less than zero will
 * cause NULL to be returned.
 * 信号量的起始值，传递小于零的值将导致返回 NULL
 *
 * @result
 * The newly created semaphore, or NULL on failure.
 * 新创建的信号量，失败时为 NULL。
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_MALLOC DISPATCH_RETURNS_RETAINED DISPATCH_WARN_RESULT
DISPATCH_NOTHROW
dispatch_semaphore_t
dispatch_semaphore_create(intptr_t value);

/*!
 * @function dispatch_semaphore_wait
 *
 * @abstract
 * Wait (decrement) for a semaphore.
 * 等待（减少）信号量。
 *
 * @discussion
 * Decrement the counting semaphore. If the resulting value is less than zero,
 * this function waits for a signal to occur before returning.
 *  减少计数信号量。如果结果值小于零，此函数将等待信号出现，然后返回。（可以使总信号量减 1，信号总量小于 0 时就会一直等待（阻塞所在线程），否则就可以正常执行。）
 *
 * @param dsema
 * The semaphore. The result of passing NULL in this parameter is undefined.
 * 信号量。在此参数中传递 NULL 的结果是未定义的。
 *
 * @param timeout
 * When to timeout (see dispatch_time). As a convenience, there are the
 * DISPATCH_TIME_NOW and DISPATCH_TIME_FOREVER constants.
 * 何时超时（dispatch_time）。为方便起见，有 DISPATCH_TIME_NOW 和 DISPATCH_TIME_FOREVER 常量。
 *
 * @result
 * Returns zero on success, or non-zero if the timeout occurred.
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
intptr_t
dispatch_semaphore_wait(dispatch_semaphore_t dsema, dispatch_time_t timeout);

/*!
 * @function dispatch_semaphore_signal
 *
 * @abstract
 * Signal (increment) a semaphore.
 * 发信号（增加）信号量。
 *
 * @discussion
 * Increment the counting semaphore. If the previous value was less than zero,
 * this function wakes a waiting thread before returning.
 * 增加计数信号量。如果先前的值小于零，则此函数在返回之前唤醒等待的线程。
 *
 * @param dsema The counting semaphore.
 * The result of passing NULL in this parameter is undefined.
 *
 * @result
 * This function returns non-zero if a thread is woken. Otherwise, zero is
 * returned.
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
intptr_t
dispatch_semaphore_signal(dispatch_semaphore_t dsema);

__END_DECLS

DISPATCH_ASSUME_NONNULL_END

#endif /* __DISPATCH_SEMAPHORE__ */
