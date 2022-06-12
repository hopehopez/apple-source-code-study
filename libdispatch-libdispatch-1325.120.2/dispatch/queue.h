/*
 * Copyright (c) 2008-2014 Apple Inc. All rights reserved.
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

#ifndef __DISPATCH_QUEUE__
#define __DISPATCH_QUEUE__

#ifndef __DISPATCH_INDIRECT__
#error "Please #include <dispatch/dispatch.h> instead of this file directly."
#include <dispatch/base.h> // for HeaderDoc
#endif

DISPATCH_ASSUME_NONNULL_BEGIN

/*!
 * @header  队列简介
 *
 * Dispatch is an abstract model for expressing concurrency via simple but
 * powerful API.
 * Dispatch 是用于通过简单但功能强大的 API 来表达并发性的抽象模型。
 *
 * At the core, dispatch provides serial FIFO queues to which blocks may be
 * submitted. Blocks submitted to these dispatch queues are invoked on a pool
 * of threads fully managed by the system. No guarantee is made regarding
 * which thread a block will be invoked on; however, it is guaranteed that only
 * one block submitted to the FIFO dispatch queue will be invoked at a time.
 * 在核心上，dispatch提供串行FIFO队列，blocks 可以提交到这些队列。
 * 提交给这些 dispatch queues 的 blocks在由系统完全管理的线程池上调用 ，无法保证将在哪个线程上调用 block（系统会自行从线程池取可用的线程）； 但是，它保证一次只调用一个提交到 FIFO dispatch queue 的 block。
 *
 * When multiple queues have blocks to be processed, the system is free to
 * allocate additional threads to invoke the blocks concurrently. When the
 * queues become empty, these threads are automatically released.
 * 当多个队列有要处理的block时，系统可以自由地分配额外的线程来并发地调用这些 blocks。 当队列变为空时，这些线程将自动释放。
 */

/*!
 * @typedef dispatch_queue_t 队列对象
 *
 * @abstract
 * Dispatch queues invoke workitems submitted to them.
 * Dispatch queues 调用提交给它们的工作项。
 
 *
 * @discussion
 * Dispatch queues come in many flavors, the most common one being the dispatch
 * serial queue (See dispatch_queue_serial_t).
 * Dispatch queues 有多种形式，最常见的一种是调度串行队列（dispatch_queue_serial_t）。
 *
 * The system manages a pool of threads which process dispatch queues and invoke
 * workitems submitted to them.
 * 系统管理一个线程池，该线程池处理 dispatch queues 并调用提交给它们的工作项
 *
 * Conceptually a dispatch queue may have its own thread of execution, and
 * interaction between queues is highly asynchronous.
 * 从概念上讲，一个 dispatch queue 可以具有自己的执行线程，并且队列之间的交互是高度异步的。
 *
 * Dispatch queues are reference counted via calls to dispatch_retain() and
 * dispatch_release(). Pending workitems submitted to a queue also hold a
 * reference to the queue until they have finished. Once all references to a
 * queue have been released, the queue will be deallocated by the system.
 * 调度队列通过调用 dispatch_retain() 和 dispatch_release() 进行引用计数。
 * 提交给队列的待处理工作项也会保留对该队列的引用，直到它们完成为止。
 * 一旦释放了对队列的所有引用，该队列将被系统销毁。
 *
 *  经过连续的宏定义整理,
 *  @protocol OS_dispatch_queue <OS_dispatch_object>
	@end

    typedef NSObject<OS_dispatch_queue> * dispatch_queue_t;
 *
 *	OS_dispatch_queue 是继承自 OS_dispatch_object 协议的协议，并且为遵循该协议的 NSObject 实例对象类型的指针定义了一个 dispatch_queue_t 的别名，dispatch_queue_t 其实就是一个遵循 OS_dispatch_queue 协议的 NSObject 实例对象指针（OC 下：dispatch_queue_t 是 NSObject 指针）。
 */
DISPATCH_DECL(dispatch_queue);

/*!
 * @typedef dispatch_queue_global_t  全局并发队列
 *
 * @abstract
 * Dispatch global concurrent queues are an abstraction around the system thread
 * pool which invokes workitems that are submitted to dispatch queues.
 * 调度全局并发队列（dispatch global concurrent queues）是围绕系统线程池的抽象， 它调用提交到调度队列的工作项。
 *
 * @discussion
 * Dispatch global concurrent queues provide buckets of priorities on top of the
 * thread pool the system manages. The system will decide how many threads
 * to allocate to this pool depending on demand and system load. In particular,
 * the system tries to maintain a good level of concurrency for this resource,
 * and will create new threads when too many existing worker threads block in
 * system calls.
 * 调度全局并发队列（dispatch global concurrent queues）在系统管理的线程池之上提供优先级桶（这个大概是哈希桶，后续看源码时再分析）.
 * 系统将根据需求和系统负载决定分配给这个池的线程数。
 * 特别是，系统会尝试为该资源保持良好的并发级别，并且当系统调用中有太多的现有工作线程阻塞时，
 * 将创建新线程。
 * （NSThread 和 GCD 的一个重大区别，GCD 下线程都是系统自动创建分配的，而 NSThread 则是自己手动创建线程或者自己手动开启线程。）
 *
 * The global concurrent queues are a shared resource and as such it is the
 * responsiblity of every user of this resource to not submit an unbounded
 * amount of work to this pool, especially work that may block, as this can
 * cause the system to spawn very large numbers of threads (aka. thread
 * explosion).
 * 全局并发队列（global concurrent queues）是共享资源，
 * 因此，此资源的每个用户都有责任不向该池提交无限数量的工作，
 * 尤其是可能阻塞的工作，因为这可能导致系统产生大量线程（
 * 又名：线程爆炸 thread explosion）。
 *
 * Work items submitted to the global concurrent queues have no ordering
 * guarantee with respect to the order of submission, and workitems submitted
 * to these queues may be invoked concurrently.
 * 提交到全局并发队列（global concurrent queues）的工作项相对于提交顺序没有排序保证，
 * 并且提交到这些队列的工作项可以并发调用（毕竟本质还是并发队列）。
 *
 * Dispatch global concurrent queues are well-known global objects that are
 * returned by dispatch_get_global_queue(). These objects cannot be modified.
 * Calls to dispatch_suspend(), dispatch_resume(), dispatch_set_context(), etc.,
 * will have no effect when used with queues of this type.
 * 调度全局并发队列（dispatch global concurrent queues）是由 dispatch_get_global_queue 函数返回的已知全局对象，这些对象无法修改。 dispatch_suspend()、dispatch_resume()、dispatch_set_context() 等函数对此类型的队列调用无效。
 */
#if defined(__DISPATCH_BUILDING_DISPATCH__) && !defined(__OBJC__)
typedef struct dispatch_queue_global_s *dispatch_queue_global_t;
#else

/*
 经过一系列转换后
 @protocol OS_dispatch_queue_global <OS_dispatch_queue>
 @end

 typedef NSObject<OS_dispatch_queue_global> * dispatch_queue_global_t;
 */
DISPATCH_DECL_SUBCLASS(dispatch_queue_global, dispatch_queue);
#endif

/*!
 * @typedef dispatch_queue_serial_t  串行队列
 *
 * @abstract
 * Dispatch serial queues invoke workitems submitted to them serially in FIFO
 * order.
 * 调度串行队列（dispatch serial queues）以 FIFO (先进先出)的顺序调用提交给它们的工作项
 *
 * @discussion
 * Dispatch serial queues are lightweight objects to which workitems may be
 * submitted to be invoked in FIFO order. A serial queue will only invoke one
 * workitem at a time, but independent serial queues may each invoke their work
 * items concurrently with respect to each other.
 * 调度串行队列（dispatch serial queues）是轻量级对象，可以向其提交工作项以 FIFO 顺序调用。串行队列一次只能调用一个工作项，但是独立的串行队列可以各自相对于彼此并发地调用其工作项。
 *
 * Serial queues can target each other (See dispatch_set_target_queue()). The
 * serial queue at the bottom of a queue hierarchy provides an exclusion
 * context: at most one workitem submitted to any of the queues in such
 * a hiearchy will run at any given time.
 * 串行队列可以相互定位（dispatch_set_target_queue()）（串行队列可以彼此作为目标）。 队列层次结构底部的串行队列提供了一个排除上下文：在任何给定的时间，提交给这种层次结构中的任何队列的最多一个工作项将运行。
 *
 *
 * Such hierarchies provide a natural construct to organize an application
 * subsystem around.
 * 这样的层次结构提供了一个自然的结构来组织应用程序子系统。
 *
 * Serial queues are created by passing a dispatch queue attribute derived from
 * DISPATCH_QUEUE_SERIAL to dispatch_queue_create_with_target().
 * 通过将调度队列属性 (DISPATCH_QUEUE_SERIAL) 传递给 dispatch_queue_create_with_target() 来创建串行队列。
 */
#if defined(__DISPATCH_BUILDING_DISPATCH__) && !defined(__OBJC__)
typedef struct dispatch_lane_s *dispatch_queue_serial_t;
#else
/* 转换宏定义后是：
 @protocol OS_dispatch_queue_serial <OS_dispatch_queue>
 @end

 typedef NSObject<OS_dispatch_queue_serial> * dispatch_queue_serial_t;
 */
DISPATCH_DECL_SUBCLASS(dispatch_queue_serial, dispatch_queue);
#endif

/*!
 * @typedef dispatch_queue_main_t  主队列
 *
 * @abstract
 * The type of the default queue that is bound to the main thread.
 * dispatch_queue_main_t 是绑定到主线程的默认队列的类型。
 *
 * @discussion
 * The main queue is a serial queue (See dispatch_queue_serial_t) which is bound
 * to the main thread of an application.
 * 主队列是一个串行队列（dispatch_queue_serial_t），该队列绑定到应用程序的主线程。
 *
 * In order to invoke workitems submitted to the main queue, the application
 * must call dispatch_main(), NSApplicationMain(), or use a CFRunLoop on the
 * main thread.
 * 为了调用提交到主队列的工作项，应用程序必须调用 dispatch_main()，NSApplicationMain() 或在主线程上使用 CFRunLoop。
 *
 * The main queue is a well known global object that is made automatically on
 * behalf of the main thread during process initialization and is returned by
 * dispatch_get_main_queue(). This object cannot be modified.  Calls to
 * dispatch_suspend(), dispatch_resume(), dispatch_set_context(), etc., will
 * have no effect when used on the main queue.
 * 主队列是一个众所周知的全局对象，它在进程初始化期间代表主线程自动创建，
 * 并由 dispatch_get_main_queue() 返回.
 * 无法修改该对象。
 * dispatch_suspend()、dispatch_resume()、dispatch_set_context() 等等函数对此类型的队列调用无效（主队列只有一个，全局并发队列有多个）
 */
#if defined(__DISPATCH_BUILDING_DISPATCH__) && !defined(__OBJC__)
typedef struct dispatch_queue_static_s *dispatch_queue_main_t;
#else
/* 转换宏定义后:
 @protocol OS_dispatch_queue_main <OS_dispatch_queue_serial>
 @end

 typedef NSObject<OS_dispatch_queue_main> * dispatch_queue_main_t;
 */
DISPATCH_DECL_SUBCLASS(dispatch_queue_main, dispatch_queue_serial);
#endif

/*!
 * @typedef dispatch_queue_concurrent_t
 *
 * @abstract
 * Dispatch concurrent queues invoke workitems submitted to them concurrently,
 * and admit a notion of barrier workitems.
 *  调度并发队列（dispatch concurrent queues）会同时调用提交给它们的工作项，
 *  并接受栅栏工作项的概念（barrier workitems 是指调用 dispatch_barrier_async 函数，向队列提交工作项））。
 *
 * @discussion
 * Dispatch concurrent queues are lightweight objects to which regular and
 * barrier workitems may be submited. Barrier workitems are invoked in
 * exclusion of any other kind of workitem in FIFO order.
 * 调度并发队列（dispatch concurrent queues）是可以向其提交常规和栅栏工作项的轻量级对象。
 * 在排除其他任何类型的工作项目（按 FIFO 顺序）时，将调用屏障工作项目。
 * （提交在 barrier 工作项之前的工作项并发执行完以后才会并发执行 barrier 工作项之后的工作项）。
 *
 * Regular workitems can be invoked concurrently for the same concurrent queue,
 * in any order. However, regular workitems will not be invoked before any
 * barrier workitem submited ahead of them has been invoked.
 * 可以对同一并发队列以任何顺序并发调用常规工作项。
 * 但是，在调用之前提交的任何barrier工作项之前，不会调用常规工作项。
 *
 * In other words, if a serial queue is equivalent to a mutex in the Dispatch
 * world, a concurrent queue is equivalent to a reader-writer lock, where
 * regular items are readers and barriers are writers.
 * 换句话说，如果在 Dispatch 世界中串行队列等效于互斥锁，则并发队列等效于 reader-writer lock，其中常规项是读取器，而barriers 是写入器。
 *
 * Concurrent queues are created by passing a dispatch queue attribute derived
 * from DISPATCH_QUEUE_CONCURRENT to dispatch_queue_create_with_target().
 * 通过将调度队列属性 DISPATCH_QUEUE_CONCURRENT 传递给 dispatch_queue_create_with_target() 来创建并发队列。
 *
 * Caveat:
 * Dispatch concurrent queues at this time do not implement priority inversion
 * avoidance when lower priority regular workitems (readers) are being invoked
 * and are preventing a higher priority barrier (writer) from being invoked.
 * 注意事项：
 * 当调用优先级较低的常规工作项（readers）时，此时调度并发队列不会实现优先级反转避免， 并且会阻止调用优先级较高的barrier（writer）。
 */
#if defined(__DISPATCH_BUILDING_DISPATCH__) && !defined(__OBJC__)
typedef struct dispatch_lane_s *dispatch_queue_concurrent_t;
#else
DISPATCH_DECL_SUBCLASS(dispatch_queue_concurrent, dispatch_queue);
#endif

__BEGIN_DECLS

/*!
 * @function dispatch_async
 *
 * @abstract
 * Submits a block for asynchronous execution on a dispatch queue.
 *
 * @discussion
 * The dispatch_async() function is the fundamental mechanism for submitting
 * blocks to a dispatch queue.
 * dispatch_async() 函数是用于将 block 提交到调度队列的基本机制
 *
 * Calls to dispatch_async() always return immediately after the block has
 * been submitted, and never wait for the block to be invoked.
 * 对 dispatch_async 函数的调用总是在 block 被提交后立即返回，而从不等待 block 被调用。
 *
 * The target queue determines whether the block will be invoked serially or
 * concurrently with respect to other blocks submitted to that same queue.
 * Serial queues are processed concurrently with respect to each other.
 * 目标队列（dispatch_queue_t queue） 决定是串行调用该block还是与其他block一起并发调用。
 * 当 queue 是并发队列时会开启多条线程并发执行所有的 block，如果 queue 是串行队列（除了主队列）的话则是仅开辟一条线程串行执行所有的 block，
 * 如果主队列的话则是不开启线程直接在主线程中串行执行所有的 block），
 * dispatch_async 函数提交 block 到不同的串行队列， 则这些串行队列是相互并行处理的。（它们在不同的线程中并发执行串行队列中的 block）
 *
 * @param queue
 * The target dispatch queue to which the block is submitted.
 * The system will hold a reference on the target queue until the block
 * has finished.
 * The result of passing NULL in this parameter is undefined.
 * block 提交到的目标调度队列。系统将在目标队列上保留引用，直到该 block 调用完成为止。在此参数中传递 NULL 的结果是不确定的。
 *
 * @param block
 * The block to submit to the target dispatch queue. This function performs
 * Block_copy() and Block_release() on behalf of callers.
 * The result of passing NULL in this parameter is undefined.
 * 提交到目标调度队列的 block。该函数代表调用者执行 Block_copy() 和 Block_release() 函数。在此参数中传递 NULL 的结果是不确定的。
 */
#ifdef __BLOCKS__
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
dispatch_async(dispatch_queue_t queue, dispatch_block_t block);
#endif

/*!
 * @function dispatch_async_f
 *
 * @abstract
 * Submits a function for asynchronous execution on a dispatch queue.
 * dispatch_async_f 提交一个函数以在调度队列上异步执行
 *
 * @discussion
 * See dispatch_async() for details.
 *
 * @param queue
 * The target dispatch queue to which the function is submitted.
 * The system will hold a reference on the target queue until the function
 * has returned.
 * The result of passing NULL in this parameter is undefined.
 * 函数被提交到的目标调度队列。系统将在目标队列上保留引用，直到函数执行完返回。
 * 在此参数中传递 NULL 的结果是不确定的。
 *
 * @param context
 * The application-defined context parameter to pass to the function.
 * 应用程序定义的上下文参数，以传递给函数，作为 work 函数执行时的参数。
 *
 * @param work
 * The application-defined function to invoke on the target queue. The first
 * parameter passed to this function is the context provided to
 * dispatch_async_f().
 * The result of passing NULL in this parameter is undefined.
 * 在目标队列上调用的应用程序定义的函数。传递给此函数的第一个参数是提供给 dispatch_async_f() 的 context 参数。
 * 在此参数中传递 NULL 的结果是不确定的。
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL1 DISPATCH_NONNULL3 DISPATCH_NOTHROW
void
dispatch_async_f(dispatch_queue_t queue,
		void *_Nullable context, dispatch_function_t work);

/*!
 * @function dispatch_sync
 *
 * @abstract
 * Submits a block for synchronous execution on a dispatch queue.
 * dispatch_sync 提交一个 block 以在调度队列上同步执行。
 *
 * @discussion
 * Submits a workitem to a dispatch queue like dispatch_async(), however
 * dispatch_sync() will not return until the workitem has finished.
 * dispatch_sync 函数用于将工作项提交到一个 dispatch queue，像 dispatch_async 函数一样，但是 dispatch_sync 在工作项完成之前不会返回。
 * (即 dispatch_sync 函数只有在提交到队列的 block 执行完成以后才会返回，会阻塞当前线程)。
 *
 * Work items submitted to a queue with dispatch_sync() do not observe certain
 * queue attributes of that queue when invoked (such as autorelease frequency
 * and QOS class).
 * 使用 dispatch_sync() 函数提交到队列的工作项在调用时不会遵守该队列的某些队列属性（例如自动释放频率和 QOS 类）。
 *
 * Calls to dispatch_sync() targeting the current queue will result
 * in dead-lock. Use of dispatch_sync() is also subject to the same
 * multi-party dead-lock problems that may result from the use of a mutex.
 * Use of dispatch_async() is preferred.
 * 针对当前队列调用 dispatch_sync 将导致死锁（dead-lock）（如在任何串行队列（包括主线程）中调用 dispatch_sync 函数提交 block 到当前串行队列，必死锁）。
 * 使用 dispatch_sync() 也会遇到由于使用互斥锁而导致的多方死锁（multi-party dead-lock）问题，最
 * 好使用 dispatch_async()。
 *
 * Unlike dispatch_async(), no retain is performed on the target queue. Because
 * calls to this function are synchronous, the dispatch_sync() "borrows" the
 * reference of the caller.
 * 与 dispatch_async() 不同，在目标队列上不执行保留。因为对这个函数的调用是同步的，所以 dispatch_sync 会 “借用” 调用者的引用
 *
 * As an optimization, dispatch_sync() invokes the workitem on the thread which
 * submitted the workitem, except when the passed queue is the main queue or
 * a queue targetting it (See dispatch_queue_main_t,
 * dispatch_set_target_queue()).
 *  作为一种优化，dispatch_sync() 在提交该工作项的线程上调用该工作项，除非所传递的队列是主队列或以其为目标的队列（参见 dispatch_queue_main_t，dispatch_set_target_queue）。
 *
 * @param queue
 * The target dispatch queue to which the block is submitted.
 * The result of passing NULL in this parameter is undefined.
 * block 提交到的目标调度队列。
 *
 * @param block
 * The block to be invoked on the target dispatch queue.
 * The result of passing NULL in this parameter is undefined.
 * 在目标调度队列上要调用的 block。
 */
#ifdef __BLOCKS__
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
dispatch_sync(dispatch_queue_t queue, DISPATCH_NOESCAPE dispatch_block_t block);
#endif

/*!
 * @function dispatch_sync_f
 *
 * @abstract
 * Submits a function for synchronous execution on a dispatch queue.
 *
 * @discussion
 * See dispatch_sync() for details.
 *
 * @param queue
 * The target dispatch queue to which the function is submitted.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param context
 * The application-defined context parameter to pass to the function.
 *
 * @param work
 * The application-defined function to invoke on the target queue. The first
 * parameter passed to this function is the context provided to
 * dispatch_sync_f().
 * The result of passing NULL in this parameter is undefined.
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL1 DISPATCH_NONNULL3 DISPATCH_NOTHROW
void
dispatch_sync_f(dispatch_queue_t queue,
		void *_Nullable context, dispatch_function_t work);

/*!
 * @function dispatch_async_and_wait
 *
 * @abstract
 * Submits a block for synchronous execution on a dispatch queue.
 *
 * @discussion
 * Submits a workitem to a dispatch queue like dispatch_async(), however
 * dispatch_async_and_wait() will not return until the workitem has finished.
 *
 * Like functions of the dispatch_sync family, dispatch_async_and_wait() is
 * subject to dead-lock (See dispatch_sync() for details).
 *
 * However, dispatch_async_and_wait() differs from functions of the
 * dispatch_sync family in two fundamental ways: how it respects queue
 * attributes and how it chooses the execution context invoking the workitem.
 *
 * <b>Differences with dispatch_sync()</b>
 *
 * Work items submitted to a queue with dispatch_async_and_wait() observe all
 * queue attributes of that queue when invoked (inluding autorelease frequency
 * or QOS class).
 *
 * When the runtime has brought up a thread to invoke the asynchronous workitems
 * already submitted to the specified queue, that servicing thread will also be
 * used to execute synchronous work submitted to the queue with
 * dispatch_async_and_wait().
 *
 * However, if the runtime has not brought up a thread to service the specified
 * queue (because it has no workitems enqueued, or only synchronous workitems),
 * then dispatch_async_and_wait() will invoke the workitem on the calling thread,
 * similar to the behaviour of functions in the dispatch_sync family.
 *
 * As an exception, if the queue the work is submitted to doesn't target
 * a global concurrent queue (for example because it targets the main queue),
 * then the workitem will never be invoked by the thread calling
 * dispatch_async_and_wait().
 *
 * In other words, dispatch_async_and_wait() is similar to submitting
 * a dispatch_block_create()d workitem to a queue and then waiting on it, as
 * shown in the code example below. However, dispatch_async_and_wait() is
 * significantly more efficient when a new thread is not required to execute
 * the workitem (as it will use the stack of the submitting thread instead of
 * requiring heap allocations).
 *
 * <code>
 *     dispatch_block_t b = dispatch_block_create(0, block);
 *     dispatch_async(queue, b);
 *     dispatch_block_wait(b, DISPATCH_TIME_FOREVER);
 *     Block_release(b);
 * </code>
 *
 * @param queue
 * The target dispatch queue to which the block is submitted.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param block
 * The block to be invoked on the target dispatch queue.
 * The result of passing NULL in this parameter is undefined.
 */
#ifdef __BLOCKS__
API_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
dispatch_async_and_wait(dispatch_queue_t queue,
		DISPATCH_NOESCAPE dispatch_block_t block);
#endif

/*!
 * @function dispatch_async_and_wait_f
 *
 * @abstract
 * Submits a function for synchronous execution on a dispatch queue.
 *
 * @discussion
 * See dispatch_async_and_wait() for details.
 *
 * @param queue
 * The target dispatch queue to which the function is submitted.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param context
 * The application-defined context parameter to pass to the function.
 *
 * @param work
 * The application-defined function to invoke on the target queue. The first
 * parameter passed to this function is the context provided to
 * dispatch_async_and_wait_f().
 * The result of passing NULL in this parameter is undefined.
 */
API_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0))
DISPATCH_EXPORT DISPATCH_NONNULL1 DISPATCH_NONNULL3 DISPATCH_NOTHROW
void
dispatch_async_and_wait_f(dispatch_queue_t queue,
		void *_Nullable context, dispatch_function_t work);


#if defined(__APPLE__) && \
		(defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && \
		__IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_7_0) || \
		(defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && \
		__MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_9)
#define DISPATCH_APPLY_AUTO_AVAILABLE 0
#define DISPATCH_APPLY_QUEUE_ARG_NULLABILITY _Nonnull
#else
#define DISPATCH_APPLY_AUTO_AVAILABLE 1
#define DISPATCH_APPLY_QUEUE_ARG_NULLABILITY _Nullable
#endif

/*!
 * @constant DISPATCH_APPLY_AUTO
 *
 * @abstract
 * Constant to pass to dispatch_apply() or dispatch_apply_f() to request that
 * the system automatically use worker threads that match the configuration of
 * the current thread as closely as possible.
 *
 * @discussion
 * When submitting a block for parallel invocation, passing this constant as the
 * queue argument will automatically use the global concurrent queue that
 * matches the Quality of Service of the caller most closely.
 *
 * No assumptions should be made about which global concurrent queue will
 * actually be used.
 *
 * Using this constant deploys backward to macOS 10.9, iOS 7.0 and any tvOS or
 * watchOS version.
 */
#if DISPATCH_APPLY_AUTO_AVAILABLE
#define DISPATCH_APPLY_AUTO ((dispatch_queue_t _Nonnull)0)
#endif

/*!
 * @function dispatch_apply
 *
 * @abstract
 * Submits a block to a dispatch queue for parallel invocation.
 *
 * @discussion
 * Submits a block to a dispatch queue for parallel invocation. This function
 * waits for the task block to complete before returning. If the specified queue
 * is concurrent, the block may be invoked concurrently, and it must therefore
 * be reentrant safe.
 *
 * Each invocation of the block will be passed the current index of iteration.
 *
 * @param iterations
 * The number of iterations to perform.
 *
 * @param queue
 * The dispatch queue to which the block is submitted.
 * The preferred value to pass is DISPATCH_APPLY_AUTO to automatically use
 * a queue appropriate for the calling thread.
 *
 * @param block
 * The block to be invoked the specified number of iterations.
 * The result of passing NULL in this parameter is undefined.
 * This function performs a Block_copy() and Block_release() of the input block
 * on behalf of the callers. To elide the additional block allocation,
 * dispatch_apply_f may be used instead.
 */
#ifdef __BLOCKS__
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL3 DISPATCH_NOTHROW
void
dispatch_apply(size_t iterations,
		dispatch_queue_t DISPATCH_APPLY_QUEUE_ARG_NULLABILITY queue,
		DISPATCH_NOESCAPE void (^block)(size_t iteration));
#endif

/*!
 * @function dispatch_apply_f
 *
 * @abstract
 * Submits a function to a dispatch queue for parallel invocation.
 *
 * @discussion
 * See dispatch_apply() for details.
 *
 * @param iterations
 * The number of iterations to perform.
 *
 * @param queue
 * The dispatch queue to which the function is submitted.
 * The preferred value to pass is DISPATCH_APPLY_AUTO to automatically use
 * a queue appropriate for the calling thread.
 *
 * @param context
 * The application-defined context parameter to pass to the function.
 *
 * @param work
 * The application-defined function to invoke on the specified queue. The first
 * parameter passed to this function is the context provided to
 * dispatch_apply_f(). The second parameter passed to this function is the
 * current index of iteration.
 * The result of passing NULL in this parameter is undefined.
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL4 DISPATCH_NOTHROW
void
dispatch_apply_f(size_t iterations,
		dispatch_queue_t DISPATCH_APPLY_QUEUE_ARG_NULLABILITY queue,
		void *_Nullable context, void (*work)(void *_Nullable context, size_t iteration));

/*!
 * @function dispatch_get_current_queue
 *
 * @abstract
 * Returns the queue on which the currently executing block is running.
 *
 * @discussion
 * Returns the queue on which the currently executing block is running.
 *
 * When dispatch_get_current_queue() is called outside of the context of a
 * submitted block, it will return the default concurrent queue.
 *
 * Recommended for debugging and logging purposes only:
 * The code must not make any assumptions about the queue returned, unless it
 * is one of the global queues or a queue the code has itself created.
 * The code must not assume that synchronous execution onto a queue is safe
 * from deadlock if that queue is not the one returned by
 * dispatch_get_current_queue().
 *
 * When dispatch_get_current_queue() is called on the main thread, it may
 * or may not return the same value as dispatch_get_main_queue(). Comparing
 * the two is not a valid way to test whether code is executing on the
 * main thread (see dispatch_assert_queue() and dispatch_assert_queue_not()).
 *
 * This function is deprecated and will be removed in a future release.
 *
 * @result
 * Returns the current queue.
 */
API_DEPRECATED("unsupported interface", macos(10.6,10.9), ios(4.0,6.0))
DISPATCH_EXPORT DISPATCH_PURE DISPATCH_WARN_RESULT DISPATCH_NOTHROW
dispatch_queue_t
dispatch_get_current_queue(void);

API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT
#if defined(__DISPATCH_BUILDING_DISPATCH__) && !defined(__OBJC__)
struct dispatch_queue_static_s _dispatch_main_q;
#else
struct dispatch_queue_s _dispatch_main_q;
#endif

/*!
 * @function dispatch_get_main_queue
 *
 * @abstract
 * Returns the default queue that is bound to the main thread.
 * dispatch_get_main_queue 返回绑定到主线程的默认队列。(_dispatch_main_q 一个全局变量，程序启动时会自动构建主线程和主队列)
 *
 * @discussion
 * In order to invoke blocks submitted to the main queue, the application must
 * call dispatch_main(), NSApplicationMain(), or use a CFRunLoop on the main
 * thread.
 * 为了调用提交到主队列的 blocks，应用程序必须调用 dispatch_main()、NSApplicationMain() 或在主线程上使用 CFRunLoop。
 *
 * The main queue is meant to be used in application context to interact with
 * the main thread and the main runloop.
 * 主队列用于在应用程序上下文中与主线程和主 runloop 进行交互。
 *
 * Because the main queue doesn't behave entirely like a regular serial queue,
 * it may have unwanted side-effects when used in processes that are not UI apps
 * (daemons). For such processes, the main queue should be avoided.
 * 由于主队列的行为不完全像常规串行队列，因此在非 UI 应用程序（守护程序）的进程中使用时，主队列可能会产生有害的副作用。对于此类过程，应避免使用主队列。
 *
 * @see dispatch_queue_main_t
 *
 * @result
 * Returns the main queue. This queue is created automatically on behalf of
 * the main thread before main() is called.
 * 返回主队列。在调用 main() 之前，该队列代表主线程自动创建。（_dispatch_main_q）
 */
DISPATCH_INLINE DISPATCH_ALWAYS_INLINE DISPATCH_CONST DISPATCH_NOTHROW
dispatch_queue_main_t
dispatch_get_main_queue(void)
{
	return DISPATCH_GLOBAL_OBJECT(dispatch_queue_main_t, _dispatch_main_q);
}

/*!
 * @typedef dispatch_queue_priority_t
 * Type of dispatch_queue_priority
 *  dispatch_queue_priority 的类型，表示队列的优先级。
 *
 * @constant DISPATCH_QUEUE_PRIORITY_HIGH
 * Items dispatched to the queue will run at high priority,
 * i.e. the queue will be scheduled for execution before
 * any default priority or low priority queue.
 * 调度到队列的项目将以高优先级运行，即队列将在任何默认优先级或低优先级队列之前被调度执行。
 *
 * @constant DISPATCH_QUEUE_PRIORITY_DEFAULT
 * Items dispatched to the queue will run at the default
 * priority, i.e. the queue will be scheduled for execution
 * after all high priority queues have been scheduled, but
 * before any low priority queues have been scheduled.
 * 调度到队列的项目将以默认优先级运行，即， 在所有高优先级队列都已调度之后，但在任何低优先级队列都已调度之前， 将调度该队列执行。
 *
 * @constant DISPATCH_QUEUE_PRIORITY_LOW
 * Items dispatched to the queue will run at low priority,
 * i.e. the queue will be scheduled for execution after all
 * default priority and high priority queues have been
 * scheduled.
 * 调度到队列的项目将以低优先级运行，即，在所有默认优先级和高优先级队列都已调度之后， 将调度该队列执行。
 *
 * @constant DISPATCH_QUEUE_PRIORITY_BACKGROUND
 * Items dispatched to the queue will run at background priority, i.e. the queue
 * will be scheduled for execution after all higher priority queues have been
 * scheduled and the system will run items on this queue on a thread with
 * background status as per setpriority(2) (i.e. disk I/O is throttled and the
 * thread's scheduling priority is set to lowest value).
 * 调度到队列的项目将在后台优先级下运行，即在所有较高优先级的队列都已调度之后，将调度该队列执行，并且系统将在线程上以 setpriority(2) 的后台状态运行该队列上的项目（即磁盘 I/O 受到限制，线程的调度优先级设置为最低值）。
 */
#define DISPATCH_QUEUE_PRIORITY_HIGH 2
#define DISPATCH_QUEUE_PRIORITY_DEFAULT 0
#define DISPATCH_QUEUE_PRIORITY_LOW (-2)
#define DISPATCH_QUEUE_PRIORITY_BACKGROUND INT16_MIN

typedef long dispatch_queue_priority_t;

/*!
 * @function dispatch_get_global_queue
 *
 * @abstract
 * Returns a well-known global concurrent queue of a given quality of service
 * class.
 * dispatch_get_global_queue 返回给定服务质量（qos_class_t）（或者 dispatch_queue_priority_t 定义的优先级）类的众所周知的全局并发队列。
 *
 * @discussion
 * See dispatch_queue_global_t.
 *
 * @param identifier
 * A quality of service class defined in qos_class_t or a priority defined in
 * dispatch_queue_priority_t.
 * identifier：在 qos_class_t 中定义的服务质量等级或在 dispatch_queue_priority_t 中定义的优先级。
 *
 * It is recommended to use quality of service class values to identify the
 * well-known global concurrent queues:
 * 建议使用服务质量类值来识别众所周知的全局并发队列：
 *  - QOS_CLASS_USER_INTERACTIVE
 *  - QOS_CLASS_USER_INITIATED
 *  - QOS_CLASS_DEFAULT
 *  - QOS_CLASS_UTILITY
 *  - QOS_CLASS_BACKGROUND
 *
 *
 * The global concurrent queues may still be identified by their priority,
 * which map to the following QOS classes:
 * 全局并发队列仍可以通过其优先级来标识，这些优先级映射到以下QOS类：
 *  - DISPATCH_QUEUE_PRIORITY_HIGH:         QOS_CLASS_USER_INITIATED
 *  - DISPATCH_QUEUE_PRIORITY_DEFAULT:      QOS_CLASS_DEFAULT
 *  - DISPATCH_QUEUE_PRIORITY_LOW:          QOS_CLASS_UTILITY
 *  - DISPATCH_QUEUE_PRIORITY_BACKGROUND:   QOS_CLASS_BACKGROUND
 *
 * @param flags
 * Reserved for future use. Passing any value other than zero may result in
 * a NULL return value.
 * flags：保留以备将来使用。传递除零以外的任何值可能会导致返回 NULL，所以日常统一传 0 就好了。
 *
 * @result
 * Returns the requested global queue or NULL if the requested global queue
 * does not exist.
 * result：返回请求的全局队列，如果请求的全局队列不存在，则返回 NULL。
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_CONST DISPATCH_WARN_RESULT DISPATCH_NOTHROW
dispatch_queue_global_t
dispatch_get_global_queue(intptr_t identifier, uintptr_t flags);

/*!
 * @typedef dispatch_queue_attr_t
 *
 * @abstract
 * Attribute for dispatch queues.
 * 调度队列的属性。
 */
DISPATCH_DECL(dispatch_queue_attr);

/*!
 * @const DISPATCH_QUEUE_SERIAL
 *
 * @discussion
 * An attribute that can be used to create a dispatch queue that invokes blocks
 * serially in FIFO order.
 * DISPATCH_QUEUE_SERIAL 宏定义，仅是一个 NULL，dispatch_queue_t serialQueue = dispatch_queue_create("com.com", DISPATCH_QUEUE_SERIAL); 日常创建串行队列必使用的一个宏，其实是 NULL。
 *
 * See dispatch_queue_serial_t.
 */
#define DISPATCH_QUEUE_SERIAL NULL

/*!
 * @const DISPATCH_QUEUE_SERIAL_INACTIVE
 *
 * @discussion
 * An attribute that can be used to create a dispatch queue that invokes blocks
 * serially in FIFO order, and that is initially inactive.
 * 可用于创建以 FIFO 顺序，顺序调用块的调度队列的属性，该属性最初是不活动的。
 * See dispatch_queue_attr_make_initially_inactive().
 */
#define DISPATCH_QUEUE_SERIAL_INACTIVE \
		dispatch_queue_attr_make_initially_inactive(DISPATCH_QUEUE_SERIAL)

/*!
 * @const DISPATCH_QUEUE_CONCURRENT
 *
 * @discussion
 * An attribute that can be used to create a dispatch queue that may invoke
 * blocks concurrently and supports barrier blocks submitted with the dispatch
 * barrier API.
 * 可用于创建调度队列的属性，该调度队列可同时调用block并支持通过调度栅栏API提交的barrier blocks。(常规 block 和 barrier 的 block 任务块)
 *
 * See dispatch_queue_concurrent_t.
 */
*
///DISPATCH_QUEUE_CONCURRENT 宏定义是把全局变量 _dispatch_queue_attr_concurrent 强制转化为了 dispatch_queue_attr_t。
#define DISPATCH_QUEUE_CONCURRENT \
		DISPATCH_GLOBAL_OBJECT(dispatch_queue_attr_t, \
		_dispatch_queue_attr_concurrent)



API_AVAILABLE(macos(10.7), ios(4.3))
DISPATCH_EXPORT
struct dispatch_queue_attr_s _dispatch_queue_attr_concurrent;

/*!
 * @const DISPATCH_QUEUE_CONCURRENT_INACTIVE
 *
 * @discussion
 * An attribute that can be used to create a dispatch queue that may invoke
 * blocks concurrently and supports barrier blocks submitted with the dispatch
 * barrier API, and that is initially inactive.
 * 可用于创建调度队列的属性，该属性可以同时调用块并支持通过调度屏障 API （dispatch_barrier_async）提交的屏障块，并且该属性最初是不活动的。
 *
 * See dispatch_queue_attr_make_initially_inactive().
 */
#define DISPATCH_QUEUE_CONCURRENT_INACTIVE \
		dispatch_queue_attr_make_initially_inactive(DISPATCH_QUEUE_CONCURRENT)

/*!
 * @function dispatch_queue_attr_make_initially_inactive
 *
 * @abstract
 * Returns an attribute value which may be provided to dispatch_queue_create()
 * or dispatch_queue_create_with_target(), in order to make the created queue
 * initially inactive.
 * dispatch_queue_attr_make_initially_inactive 返回一个属性值，该值可提供给 dispatch_queue_create 或 dispatch_queue_create_with_target，以便使创建的队列最初处于非活动状态。
 *
 * @discussion
 * Dispatch queues may be created in an inactive state. Queues in this state
 * have to be activated before any blocks associated with them will be invoked.
 * 调度队列可以在非活动状态下创建。必须先激活处于这种状态的队列，然后才能调用与其关联的任何 blocks。
 *
 * A queue in inactive state cannot be deallocated, dispatch_activate() must be
 * called before the last reference to a queue created with this attribute is
 * released.
 * 无法释放处于非活动状态的队列，必须在释放使用此属性创建的队列的最后一个引用之前调用 dispatch_activate()。
 *
 * The target queue of a queue in inactive state can be changed using
 * dispatch_set_target_queue(). Change of target queue is no longer permitted
 * once an initially inactive queue has been activated.
 * 可以使用 dispatch_set_target_queue() 更改处于非活动状态的队列的目标队列。一旦最初不活动的队列被激活，就不再允许更改目标队列。
 *
 * @param attr
 * A queue attribute value to be combined with the initially inactive attribute.
 * 队列属性值要与最初不活动的属性组合。
 *
 * @return
 * Returns an attribute value which may be provided to dispatch_queue_create()
 * and dispatch_queue_create_with_target().
 * The new value combines the attributes specified by the 'attr' parameter with
 * the initially inactive attribute.
 * 返回可以提供给 dispatch_queue_create 和 dispatch_queue_create_with_target 的属性值。新值将 “attr” 参数指定的属性与最初处于非活动状态的属性结合在一起。
 */
API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
DISPATCH_EXPORT DISPATCH_WARN_RESULT DISPATCH_PURE DISPATCH_NOTHROW
dispatch_queue_attr_t
dispatch_queue_attr_make_initially_inactive(
		dispatch_queue_attr_t _Nullable attr);

/*!
 * @const DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL
 *
 * @discussion
 * A dispatch queue created with this attribute invokes blocks serially in FIFO
 * order, and surrounds execution of any block submitted asynchronously to it
 * with the equivalent of a individual Objective-C <code>@autoreleasepool</code>
 * scope.
 * 使用此属性创建的调度队列按 FIFO 顺序串行调用block，并用相当于单个 Objective-C @autoreleasepool 作用域来包围异步提交给它的任何块的执行。
 *
 * See dispatch_queue_attr_make_with_autorelease_frequency().
 */
#define DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL \
		dispatch_queue_attr_make_with_autorelease_frequency(\
				DISPATCH_QUEUE_SERIAL, DISPATCH_AUTORELEASE_FREQUENCY_WORK_ITEM)

/*!
 * @const DISPATCH_QUEUE_CONCURRENT_WITH_AUTORELEASE_POOL
 *
 * @discussion
 * A dispatch queue created with this attribute may invokes blocks concurrently
 * and supports barrier blocks submitted with the dispatch barrier API. It also
 * surrounds execution of any block submitted asynchronously to it with the
 * equivalent of a individual Objective-C <code>@autoreleasepool</code>
 * 使用此属性创建的调度队列可以并发调用block，并支持使用 dispatch barrier API 提交的 barrier blocks。它还包围了异步提交给它的任何block的执行，这些block相当于一个单独的 Objective-C @autoreleasepool。
 *
 * See dispatch_queue_attr_make_with_autorelease_frequency().
 */
#define DISPATCH_QUEUE_CONCURRENT_WITH_AUTORELEASE_POOL \
		dispatch_queue_attr_make_with_autorelease_frequency(\
				DISPATCH_QUEUE_CONCURRENT, DISPATCH_AUTORELEASE_FREQUENCY_WORK_ITEM)

/*!
 * @typedef dispatch_autorelease_frequency_t
 * Values to pass to the dispatch_queue_attr_make_with_autorelease_frequency()
 * function.
 *
 * @const DISPATCH_AUTORELEASE_FREQUENCY_INHERIT
 * Dispatch queues with this autorelease frequency inherit the behavior from
 * their target queue. This is the default behavior for manually created queues.
 *
 * @const DISPATCH_AUTORELEASE_FREQUENCY_WORK_ITEM
 * Dispatch queues with this autorelease frequency push and pop an autorelease
 * pool around the execution of every block that was submitted to it
 * asynchronously.
 * @see dispatch_queue_attr_make_with_autorelease_frequency().
 *
 * @const DISPATCH_AUTORELEASE_FREQUENCY_NEVER
 * Dispatch queues with this autorelease frequency never set up an individual
 * autorelease pool around the execution of a block that is submitted to it
 * asynchronously. This is the behavior of the global concurrent queues.
 */
DISPATCH_ENUM(dispatch_autorelease_frequency, unsigned long,
	DISPATCH_AUTORELEASE_FREQUENCY_INHERIT DISPATCH_ENUM_API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0)) = 0,
	DISPATCH_AUTORELEASE_FREQUENCY_WORK_ITEM DISPATCH_ENUM_API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0)) = 1,
	DISPATCH_AUTORELEASE_FREQUENCY_NEVER DISPATCH_ENUM_API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0)) = 2,
);

/*!
 * @function dispatch_queue_attr_make_with_autorelease_frequency
 *
 * @abstract
 * Returns a dispatch queue attribute value with the autorelease frequency
 * set to the specified value.
 *
 * @discussion
 * When a queue uses the per-workitem autorelease frequency (either directly
 * or inherithed from its target queue), any block submitted asynchronously to
 * this queue (via dispatch_async(), dispatch_barrier_async(),
 * dispatch_group_notify(), etc...) is executed as if surrounded by a individual
 * Objective-C <code>@autoreleasepool</code> scope.
 *
 * Autorelease frequency has no effect on blocks that are submitted
 * synchronously to a queue (via dispatch_sync(), dispatch_barrier_sync()).
 *
 * The global concurrent queues have the DISPATCH_AUTORELEASE_FREQUENCY_NEVER
 * behavior. Manually created dispatch queues use
 * DISPATCH_AUTORELEASE_FREQUENCY_INHERIT by default.
 *
 * Queues created with this attribute cannot change target queues after having
 * been activated. See dispatch_set_target_queue() and dispatch_activate().
 *
 * @param attr
 * A queue attribute value to be combined with the specified autorelease
 * frequency or NULL.
 *
 * @param frequency
 * The requested autorelease frequency.
 *
 * @return
 * Returns an attribute value which may be provided to dispatch_queue_create()
 * or NULL if an invalid autorelease frequency was requested.
 * This new value combines the attributes specified by the 'attr' parameter and
 * the chosen autorelease frequency.
 */
API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
DISPATCH_EXPORT DISPATCH_WARN_RESULT DISPATCH_PURE DISPATCH_NOTHROW
dispatch_queue_attr_t
dispatch_queue_attr_make_with_autorelease_frequency(
		dispatch_queue_attr_t _Nullable attr,
		dispatch_autorelease_frequency_t frequency);

/*!
 * @function dispatch_queue_attr_make_with_qos_class
 *
 * @abstract
 * Returns an attribute value which may be provided to dispatch_queue_create()
 * or dispatch_queue_create_with_target(), in order to assign a QOS class and
 * relative priority to the queue.
 * 返回可以提供给 dispatch_queue_create 或 dispatch_queue_create_with_target 的属性值，以便为队列分配 QOS 类（形参：qos_class）和相对优先级（形参：relative_priority）。
 *
 * @discussion
 * When specified in this manner, the QOS class and relative priority take
 * precedence over those inherited from the dispatch queue's target queue (if
 * any) as long that does not result in a lower QOS class and relative priority.
 * 如果以此方式指定，则 QOS 类和相对优先级优先于从调度队列的目标队列（如果有）继承的优先级，只要不会导致较低的 QOS 类和相对优先级即可。
 *
 * The global queue priorities map to the following QOS classes:
 *  全局队列优先级映射到以下 QOS 类：
 *  - DISPATCH_QUEUE_PRIORITY_HIGH:         QOS_CLASS_USER_INITIATED
 *  - DISPATCH_QUEUE_PRIORITY_DEFAULT:      QOS_CLASS_DEFAULT
 *  - DISPATCH_QUEUE_PRIORITY_LOW:          QOS_CLASS_UTILITY
 *  - DISPATCH_QUEUE_PRIORITY_BACKGROUND:   QOS_CLASS_BACKGROUND
 *
 * Example: 例如：
 * <code>
 *	dispatch_queue_t queue;
 *	dispatch_queue_attr_t attr;
 *
 *	//实参是 DISPATCH_QUEUE_SERIAL 和 QOS_CLASS_UTILITY
 *	attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,
 *			QOS_CLASS_UTILITY, 0);
 *	queue = dispatch_queue_create("com.example.myqueue", attr);
 * </code>
 *
 * The QOS class and relative priority set this way on a queue have no effect on
 * blocks that are submitted synchronously to a queue (via dispatch_sync(),
 * dispatch_barrier_sync()).
 * 以这种方式在队列上设置的 QOS 类和相对优先级对同步提交到队列的block没有影响（通过 dispatch_sync、dispatch_barrier_sync）。
 *
 * @param attr
 * A queue attribute value to be combined with the QOS class, or NULL.
 * 要与 QOS 类组合的队列属性值，或者为 NULL。
 *
 * @param qos_class
 * A QOS class value:
 *  - QOS_CLASS_USER_INTERACTIVE
 *  - QOS_CLASS_USER_INITIATED
 *  - QOS_CLASS_DEFAULT
 *  - QOS_CLASS_UTILITY
 *  - QOS_CLASS_BACKGROUND
 * Passing any other value results in NULL being returned.
 * QOS 类值，只能传递上面现有的枚举值，传递任何其他值都会导致返回 NULL。
 *
 * @param relative_priority
 * A relative priority within the QOS class. This value is a negative
 * offset from the maximum supported scheduler priority for the given class.
 * Passing a value greater than zero or less than QOS_MIN_RELATIVE_PRIORITY
 * results in NULL being returned.
 * QOS 类中的相对优先级。该值是给定类别与最大支持的调度程序优先级的负偏移量，传递大于零或小于 QOS_MIN_RELATIVE_PRIORITY（-15）的值将导致返回 NULL。
 *
 * @return
 * Returns an attribute value which may be provided to dispatch_queue_create()
 * and dispatch_queue_create_with_target(), or NULL if an invalid QOS class was
 * requested.
 * The new value combines the attributes specified by the 'attr' parameter and
 * the new QOS class and relative priority.
 * 返回可以提供给 dispatch_queue_create 和 dispatch_queue_create_with_target 的属性值；如果请求了无效的 QOS 类，则返回 NULL。新值结合了 attr 参数指定的属性，新的 QOS 类（形参：qos_class）和相对优先级（形参：relative_priority）。
 */
API_AVAILABLE(macos(10.10), ios(8.0))
DISPATCH_EXPORT DISPATCH_WARN_RESULT DISPATCH_PURE DISPATCH_NOTHROW
dispatch_queue_attr_t
dispatch_queue_attr_make_with_qos_class(dispatch_queue_attr_t _Nullable attr,
		dispatch_qos_class_t qos_class, int relative_priority);

/*!
 * @const DISPATCH_TARGET_QUEUE_DEFAULT
 * @discussion Constant to pass to the dispatch_queue_create_with_target(),
 * dispatch_set_target_queue() and dispatch_source_create() functions to
 * indicate that the default target queue for the object type in question
 * should be used.
 * 传递给 dispatch_queue_create_with_target()、dispatch_set_target_queue() 和 dispatch_source_create() 函数的常量，以指示应使用相关对象类型的默认目标队列。
 */
#define DISPATCH_TARGET_QUEUE_DEFAULT NULL

/*!
 * @function dispatch_queue_create_with_target
 *
 * @abstract
 * Creates a new dispatch queue with a specified target queue.
 * 用指定的目标队列创建一个新的调度队列。
 *
 * @discussion
 * Dispatch queues created with the DISPATCH_QUEUE_SERIAL or a NULL attribute
 * invoke blocks serially in FIFO order.
 *
 * Dispatch queues created with the DISPATCH_QUEUE_CONCURRENT attribute may
 * invoke blocks concurrently (similarly to the global concurrent queues, but
 * potentially with more overhead), and support barrier blocks submitted with
 * the dispatch barrier API, which e.g. enables the implementation of efficient
 * reader-writer schemes.
 *
 * When a dispatch queue is no longer needed, it should be released with
 * dispatch_release(). Note that any pending blocks submitted asynchronously to
 * a queue will hold a reference to that queue. Therefore a queue will not be
 * deallocated until all pending blocks have finished.
 *
 * When using a dispatch queue attribute @a attr specifying a QoS class (derived
 * from the result of dispatch_queue_attr_make_with_qos_class()), passing the
 * result of dispatch_get_global_queue() in @a target will ignore the QoS class
 * of that global queue and will use the global queue with the QoS class
 * specified by attr instead.
 *
 * Queues created with dispatch_queue_create_with_target() cannot have their
 * target queue changed, unless created inactive (See
 * dispatch_queue_attr_make_initially_inactive()), in which case the target
 * queue can be changed until the newly created queue is activated with
 * dispatch_activate().
 *
 * @param label
 * A string label to attach to the queue.
 * This parameter is optional and may be NULL.
 *
 * @param attr
 * A predefined attribute such as DISPATCH_QUEUE_SERIAL,
 * DISPATCH_QUEUE_CONCURRENT, or the result of a call to
 * a dispatch_queue_attr_make_with_* function.
 *
 * @param target
 * The target queue for the newly created queue. The target queue is retained.
 * If this parameter is DISPATCH_TARGET_QUEUE_DEFAULT, sets the queue's target
 * queue to the default target queue for the given queue type.
 *
 * @result
 * The newly created dispatch queue.
 */
API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
DISPATCH_EXPORT DISPATCH_MALLOC DISPATCH_RETURNS_RETAINED DISPATCH_WARN_RESULT
DISPATCH_NOTHROW
dispatch_queue_t
dispatch_queue_create_with_target(const char *_Nullable label,
		dispatch_queue_attr_t _Nullable attr, dispatch_queue_t _Nullable target)
		DISPATCH_ALIAS_V2(dispatch_queue_create_with_target);

/*!
 * @function dispatch_queue_create
 *
 * @abstract
 * Creates a new dispatch queue to which blocks may be submitted.
 * 创建一个新的调度队列，可以向其提交block
 *
 * @discussion
 * Dispatch queues created with the DISPATCH_QUEUE_SERIAL or a NULL attribute
 * invoke blocks serially in FIFO order.
 * 使用 DISPATCH_QUEUE_SERIAL 或 NULL 属性创建的调度队列按 FIFO 顺序依次调用块。
 *
 * Dispatch queues created with the DISPATCH_QUEUE_CONCURRENT attribute may
 * invoke blocks concurrently (similarly to the global concurrent queues, but
 * potentially with more overhead), and support barrier blocks submitted with
 * the dispatch barrier API, which e.g. enables the implementation of efficient
 * reader-writer schemes.
 * 使用 DISPATCH_QUEUE_CONCURRENT 属性创建的调度队列可以并发调用block（类似于全局并发队列（dispatch_get_global_queue 函数获取的队列），但可能会有更多开销），并支持通过barrier API （dispatch_barrier_async 函数）提交的屏障块（blocks），例如实现有效的读写器方案（多读单写模型）。
 *
 * When a dispatch queue is no longer needed, it should be released with
 * dispatch_release(). Note that any pending blocks submitted asynchronously to
 * a queue will hold a reference to that queue. Therefore a queue will not be
 * deallocated until all pending blocks have finished.
 * 当不再需要调度队列时，应使用 dispatch_release 释放它。
 * 请注意，异步提交到队列的任何待处理块（pending blocks）都将保存对该队列的引用。
 * 因此，在所有待处理块（pending blocks）都完成之前，不会释放队列
 *
 * Passing the result of the dispatch_queue_attr_make_with_qos_class() function
 * to the attr parameter of this function allows a quality of service class and
 * relative priority to be specified for the newly created queue.
 * The quality of service class so specified takes precedence over the quality
 * of service class of the newly created dispatch queue's target queue (if any)
 * as long that does not result in a lower QOS class and relative priority.
 * 通过将dispatch_queue_attr_make_with_qos_class() 函数的结果传递给此函数的 attr 参数，
 * 可以为新创建的队列指定服务质量类（dispatch_qos_class_t qos_class）和相对优先级（int relative_priority）。
 * 这样指定的服务质量等级优先于新创建的调度队列的目标队列（如果有）的服务质量等级， 只要这不会导致较低的 QOS 类和相对优先级
 *
 * When no quality of service class is specified, the target queue of a newly
 * created dispatch queue is the default priority global concurrent queue.
 * 如果未指定服务质量等级，则新创建的调度队列的目标队列是默认优先级的全局并发队列。
 *
 * @param label
 * A string label to attach to the queue.
 * This parameter is optional and may be NULL.
 * 附加到队列的字符串标签。此参数是可选的，可以为 NULL。
 *
 * @param attr
 * A predefined attribute such as DISPATCH_QUEUE_SERIAL,
 * DISPATCH_QUEUE_CONCURRENT, or the result of a call to
 * a dispatch_queue_attr_make_with_* function.
 * 预定义的属性，例如 DISPATCH_QUEUE_SERIAL，DISPATCH_QUEUE_CONCURRENT 或调用 dispatch_queue_attr_make_with_ * 函数的结果。
 *
 * @result
 * The newly created dispatch queue.
 * 返回新创建的 dispatch queue。
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_MALLOC DISPATCH_RETURNS_RETAINED DISPATCH_WARN_RESULT
DISPATCH_NOTHROW
dispatch_queue_t
dispatch_queue_create(const char *_Nullable label,
		dispatch_queue_attr_t _Nullable attr);

/*!
 * @const DISPATCH_CURRENT_QUEUE_LABEL
 * @discussion Constant to pass to the dispatch_queue_get_label() function to
 * retrieve the label of the current queue.
 * 传递给 dispatch_queue_get_label() 函数的常量，以检索当前队列的标签。
 */
#define DISPATCH_CURRENT_QUEUE_LABEL NULL

/*!
 * @function dispatch_queue_get_label
 *
 * @abstract
 * Returns the label of the given queue, as specified when the queue was
 * created, or the empty string if a NULL label was specified.
 * 返回在创建队列时指定的给定队列的标签，如果指定了 NULL 标签，则返回空字符串。
 *
 * Passing DISPATCH_CURRENT_QUEUE_LABEL will return the label of the current
 * queue.
 * 传递 DISPATCH_CURRENT_QUEUE_LABEL 将返回当前队列的标签。
 *
 * @param queue
 * The queue to query, or DISPATCH_CURRENT_QUEUE_LABEL.
 *
 * @result
 * The label of the queue.
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_PURE DISPATCH_WARN_RESULT DISPATCH_NOTHROW
const char *
dispatch_queue_get_label(dispatch_queue_t _Nullable queue);

/*!
 * @function dispatch_queue_get_qos_class
 *
 * @abstract
 * Returns the QOS class and relative priority of the given queue.
 * 返回给定队列的 QOS 类和相对优先级
 *
 * @discussion
 * If the given queue was created with an attribute value returned from
 * dispatch_queue_attr_make_with_qos_class(), this function returns the QOS
 * class and relative priority specified at that time; for any other attribute
 * value it returns a QOS class of QOS_CLASS_UNSPECIFIED and a relative
 * priority of 0.
 *
 * 如果给定队列是使用从 dispatch_queue_attr_make_with_qos_class 返回的属性值创建的，则此函数返回当时指定的 QOS 类和相对优先级；对于任何其他属性值，它将返回 QOS_CLASS_UNSPECIFIED 的 QOS 类和相对优先级 0。
 *
 * If the given queue is one of the global queues, this function returns its
 * assigned QOS class value as documented under dispatch_get_global_queue() and
 * a relative priority of 0; in the case of the main queue it returns the QOS
 * value provided by qos_class_main() and a relative priority of 0.
 * 如果给定队列是全局队列之一，则此函数返回在 dispatch_get_global_queue 下记录的已分配 QOS 类值，相对优先级为 0；否则，返回 0。对于主队列，它返回由 qos_class_main 提供的 QOS 值和相对优先级 0。
 *
 * @param queue
 * The queue to query.
 * 要查询的队列。
 *
 * @param relative_priority_ptr
 * A pointer to an int variable to be filled with the relative priority offset
 * within the QOS class, or NULL.
 * 指向 int 变量的指针，该变量将用 QOS 类中的相对优先级偏移或 NULL 填充。
 *
 * @return
 * A QOS class value:
 *	- QOS_CLASS_USER_INTERACTIVE
 *	- QOS_CLASS_USER_INITIATED
 *	- QOS_CLASS_DEFAULT
 *	- QOS_CLASS_UTILITY
 *	- QOS_CLASS_BACKGROUND
 *	- QOS_CLASS_UNSPECIFIED
 */
API_AVAILABLE(macos(10.10), ios(8.0))
DISPATCH_EXPORT DISPATCH_WARN_RESULT DISPATCH_NONNULL1 DISPATCH_NOTHROW
dispatch_qos_class_t
dispatch_queue_get_qos_class(dispatch_queue_t queue,
		int *_Nullable relative_priority_ptr);

/*!
 * @function dispatch_set_target_queue
 *
 * @abstract
 * Sets the target queue for the given object.
 * 设置给定对象的目标队列
 *
 * @discussion
 * An object's target queue is responsible for processing the object.
 * 对象的目标队列负责处理对象。
 *
 * When no quality of service class and relative priority is specified for a
 * dispatch queue at the time of creation, a dispatch queue's quality of service
 * class is inherited from its target queue. The dispatch_get_global_queue()
 * function may be used to obtain a target queue of a specific quality of
 * service class, however the use of dispatch_queue_attr_make_with_qos_class()
 * is recommended instead.
 * 如果在创建时未为调度队列指定服务质量等级和相对优先级，则调度队列的服务质量等级将从其目标队列继承。 dispatch_get_global_queue 函数可用于获取特定服务质量类的目标队列，但是建议改为使用 dispatch_queue_attr_make_with_qos_class。

 *
 * Blocks submitted to a serial queue whose target queue is another serial
 * queue will not be invoked concurrently with blocks submitted to the target
 * queue or to any other queue with that same target queue.
 * 提交到目标队列是另一个串行队列的串行队列的块不会与提交到目标队列或具有相同目标队列的任何其他队列的块同时调用。
 *
 * The result of introducing a cycle into the hierarchy of target queues is
 * undefined.
 * 将循环引入目标队列的层次结构的结果是不确定的。
 *
 * A dispatch source's target queue specifies where its event handler and
 * cancellation handler blocks will be submitted.
 * 调度源的目标队列指定将其事件处理程序和取消处理程序块提交到的位置。
 *
 * A dispatch I/O channel's target queue specifies where where its I/O
 * operations are executed. If the channel's target queue's priority is set to
 * DISPATCH_QUEUE_PRIORITY_BACKGROUND, then the I/O operations performed by
 * dispatch_io_read() or dispatch_io_write() on that queue will be
 * throttled when there is I/O contention.
 * 调度 I/O 通道的目标队列指定在何处执行其 I/O 操作。如果通道的目标队列的优先级设置为 DISPATCH_QUEUE_PRIORITY_BACKGROUND，则当存在 I/O 争用时，将限制该队列上 dispatch_io_read 或 dispatch_io_write 执行的 I/O 操作。
 *
 * For all other dispatch object types, the only function of the target queue
 * is to determine where an object's finalizer function is invoked.
 * 对于所有其他调度对象类型，目标队列的唯一功能是确定在何处调用对象的终结函数（object's finalizer function）。
 *
 * In general, changing the target queue of an object is an asynchronous
 * operation that doesn't take effect immediately, and doesn't affect blocks
 * already associated with the specified object.
 * 通常，更改对象的目标队列是异步操作，不会立即生效，也不会影响已经与指定对象关联的block。
 *
 * However, if an object is inactive at the time dispatch_set_target_queue() is
 * called, then the target queue change takes effect immediately, and will
 * affect blocks already associated with the specified object. After an
 * initially inactive object has been activated, calling
 * dispatch_set_target_queue() results in an assertion and the process being
 * terminated.
 * 但是，如果在调用 dispatch_set_target_queue 时某个对象处于非活动状态，则目标队列更改将立即生效，并将影响已经与指定对象关联的块。激活最初不活动的对象后，调用 dispatch_set_target_queue 会导致声明并终止过程。
 *
 * If a dispatch queue is active and targeted by other dispatch objects,
 * changing its target queue results in undefined behavior.
 * 如果调度队列处于活动状态并被其他调度对象作为目标，则更改其目标队列将导致未定义的行为。
 *
 * @param object
 * The object to modify.
 * The result of passing NULL in this parameter is undefined.
 * 要修改的对象。在此参数中传递 NULL 的结果是不确定的。
 *
 * @param queue
 * The new target queue for the object. The queue is retained, and the
 * previous target queue, if any, is released.
 * If queue is DISPATCH_TARGET_QUEUE_DEFAULT, set the object's target queue
 * to the default target queue for the given object type.
 * 对象的新目标队列。保留队列，并释放先前的目标队列（如果有）。
 * 如果 queue 是 DISPATCH_TARGET_QUEUE_DEFAULT， 则将给定对象类型的对象目标队列设置为默认目标队列。
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NOTHROW
void
dispatch_set_target_queue(dispatch_object_t object,
		dispatch_queue_t _Nullable queue);

/*!
 * @function dispatch_main
 *
 * @abstract
 * Execute blocks submitted to the main queue.
 * 执行提交到主队列的块。
 *
 * @discussion
 * This function "parks" the main thread and waits for blocks to be submitted
 * to the main queue. This function never returns.
 * 此函数 “驻留” 主线程，并等待将块提交到主队列，该函数从不返回。
 *
 * Applications that call NSApplicationMain() or CFRunLoopRun() on the
 * main thread do not need to call dispatch_main().
 * 主线程上调用 NSApplicationMain() 或 CFRunLoopRun() 的应用程序无需调用 dispatch_main()。
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NOTHROW DISPATCH_NORETURN
void
dispatch_main(void);

/*!
 * @function dispatch_after
 *
 * @abstract
 * Schedule a block for execution on a given queue at a specified time.
 * 安排一个 block 在指定时间后在给定队列上执行。
 *
 *
 * @discussion
 * Passing DISPATCH_TIME_NOW as the "when" parameter is supported, but not as
 * optimal as calling dispatch_async() instead. Passing DISPATCH_TIME_FOREVER
 * is undefined.
 *
 * @param when
 * A temporal milestone returned by dispatch_time() or dispatch_walltime().
 *
 * @param queue
 * A queue to which the given block will be submitted at the specified time.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param block
 * The block of code to execute.
 * The result of passing NULL in this parameter is undefined.
 */
#ifdef __BLOCKS__
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL2 DISPATCH_NONNULL3 DISPATCH_NOTHROW
void
dispatch_after(dispatch_time_t when, dispatch_queue_t queue,
		dispatch_block_t block);
#endif

/*!
 * @function dispatch_after_f
 *
 * @abstract
 * Schedule a function for execution on a given queue at a specified time.
 *
 * @discussion
 * See dispatch_after() for details.
 *
 * @param when
 * A temporal milestone returned by dispatch_time() or dispatch_walltime().
 *
 * @param queue
 * A queue to which the given function will be submitted at the specified time.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param context
 * The application-defined context parameter to pass to the function.
 *
 * @param work
 * The application-defined function to invoke on the target queue. The first
 * parameter passed to this function is the context provided to
 * dispatch_after_f().
 * The result of passing NULL in this parameter is undefined.
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL2 DISPATCH_NONNULL4 DISPATCH_NOTHROW
void
dispatch_after_f(dispatch_time_t when, dispatch_queue_t queue,
		void *_Nullable context, dispatch_function_t work);

/*!
 * @functiongroup Dispatch Barrier API
 * The dispatch barrier API is a mechanism for submitting barrier blocks to a
 * dispatch queue, analogous to the dispatch_async()/dispatch_sync() API.
 * Dispatch barrier API 是一种机制，用于将屏障块（barrier blocks）提交给调度队列，类似于 dispatch_async()/dispatch_sync() API。
 *
 * It enables the implementation of efficient reader/writer schemes.
 * 它可以实现有效的读取器/写入器方案。
 *
 * Barrier blocks only behave specially when submitted to queues created with
 * the DISPATCH_QUEUE_CONCURRENT attribute; on such a queue, a barrier block
 * will not run until all blocks submitted to the queue earlier have completed,
 * and any blocks submitted to the queue after a barrier block will not run
 * until the barrier block has completed.
 * 屏障块仅在​​提交给使用 DISPATCH_QUEUE_CONCURRENT 属性创建的队列时才表现出特殊的行为。
 * 在这样的队列上，屏障块将不会运行，直到更早提交给队列的所有块都已完成， 并且屏障块之后提交给队列的任何块都将不会运行，直到屏障块已完成。
 *
 * When submitted to a a global queue or to a queue not created with the
 * DISPATCH_QUEUE_CONCURRENT attribute, barrier blocks behave identically to
 * blocks submitted with the dispatch_async()/dispatch_sync() API.
 * 当提交到全局队列或未使用 DISPATCH_QUEUE_CONCURRENT 属性创建的队列时，屏障块的行为与使用 dispatch_async/dispatch_sync API 提交的块相同。
 * （如果使用 dispatch_async 和 dispatch_barrier_async 提交到 dispatch_get_global_queue 取得的 queue，则并发执行，失去屏障的功能。）
 *
 */

/*!
 * @function dispatch_barrier_async
 *
 * @abstract
 * Submits a barrier block for asynchronous execution on a dispatch queue.
 * 提交 barrier block 以在调度队列上异步执行。（同 dispatch_async 不会阻塞当前线程，直接返回执行接下来的语句，但是后添加的 block 则是等到 barrier block 执行完成后才会开始执行。）
 *
 * @discussion
 * Submits a block to a dispatch queue like dispatch_async(), but marks that
 * block as a barrier (relevant only on DISPATCH_QUEUE_CONCURRENT queues).
 * 将 \一个块提交到诸如 dispatch_async 之类的调度队列中，但将该块标记为屏障（barrier）（仅与 DISPATCH_QUEUE_CONCURRENT 队列相关）。
 *
 * See dispatch_async() for details and "Dispatch Barrier API" for a description
 * of the barrier semantics.
 *
 *
 * @param queue
 * The target dispatch queue to which the block is submitted.
 * The system will hold a reference on the target queue until the block
 * has finished.
 * The result of passing NULL in this parameter is undefined.
 * 块提交到的目标调度队列。系统将在目标队列上保留引用，直到该块完成为止。在此参数中传递 NULL 的结果是不确定的。
 *
 * @param block
 * The block to submit to the target dispatch queue. This function performs
 * Block_copy() and Block_release() on behalf of callers.
 * The result of passing NULL in this parameter is undefined.
 * 提交到目标调度队列的块。该函数代表调用者执行 Block_copy 和 Block_release。在此参数中传递 NULL 的结果是不确定的。
 */
#ifdef __BLOCKS__
API_AVAILABLE(macos(10.7), ios(4.3))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
dispatch_barrier_async(dispatch_queue_t queue, dispatch_block_t block);
#endif

/*!
 * @function dispatch_barrier_async_f
 *
 * @abstract
 * Submits a barrier function for asynchronous execution on a dispatch queue.
 * 同上，只是把 block 换成了函数。
 *
 * @discussion
 * Submits a function to a dispatch queue like dispatch_async_f(), but marks
 * that function as a barrier (relevant only on DISPATCH_QUEUE_CONCURRENT
 * queues).
 *
 * See dispatch_async_f() for details and "Dispatch Barrier API" for a
 * description of the barrier semantics.
 *
 * @param queue
 * The target dispatch queue to which the function is submitted.
 * The system will hold a reference on the target queue until the function
 * has returned.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param context
 * The application-defined context parameter to pass to the function.
 *
 * @param work
 * The application-defined function to invoke on the target queue. The first
 * parameter passed to this function is the context provided to
 * dispatch_barrier_async_f().
 * The result of passing NULL in this parameter is undefined.
 */
API_AVAILABLE(macos(10.7), ios(4.3))
DISPATCH_EXPORT DISPATCH_NONNULL1 DISPATCH_NONNULL3 DISPATCH_NOTHROW
void
dispatch_barrier_async_f(dispatch_queue_t queue,
		void *_Nullable context, dispatch_function_t work);

/*!
 * @function dispatch_barrier_sync
 *
 * @abstract
 * Submits a barrier block for synchronous execution on a dispatch queue.
 * 提交屏障块（barrier block）以在调度队列上同步执行（会阻塞当前线程，直到 barrier block 执行完成才会返回）。
 *
 * @discussion
 * Submits a block to a dispatch queue like dispatch_sync(), but marks that
 * block as a barrier (relevant only on DISPATCH_QUEUE_CONCURRENT queues).
 * 将一个块提交到诸如 dispatch_sync之类的调度队列中（阻塞当前线程，直到 block 执行完毕才会返回），但将该块标记为屏障（仅与 DISPATCH_QUEUE_CONCURRENT 队列相关）。
 *
 * See dispatch_sync() for details and "Dispatch Barrier API" for a description
 * of the barrier semantics.
 *
 * @param queue
 * The target dispatch queue to which the block is submitted.
 * The result of passing NULL in this parameter is undefined.
 * 块提交到的目标调度队列。在此参数中传递 NULL 的结果是不确定的。
 *
 * @param block
 * The block to be invoked on the target dispatch queue.
 * The result of passing NULL in this parameter is undefined.
 * 在目标调度队列上要调用的块。在此参数中传递 NULL 的结果是不确定的。
 */
#ifdef __BLOCKS__
API_AVAILABLE(macos(10.7), ios(4.3))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
dispatch_barrier_sync(dispatch_queue_t queue,
		DISPATCH_NOESCAPE dispatch_block_t block);
#endif

/*!
 * @function dispatch_barrier_sync_f
 *
 * @abstract
 * Submits a barrier function for synchronous execution on a dispatch queue.
 *
 * @discussion
 * Submits a function to a dispatch queue like dispatch_sync_f(), but marks that
 * fuction as a barrier (relevant only on DISPATCH_QUEUE_CONCURRENT queues).
 *
 * See dispatch_sync_f() for details.
 *
 * @param queue
 * The target dispatch queue to which the function is submitted.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param context
 * The application-defined context parameter to pass to the function.
 *
 * @param work
 * The application-defined function to invoke on the target queue. The first
 * parameter passed to this function is the context provided to
 * dispatch_barrier_sync_f().
 * The result of passing NULL in this parameter is undefined.
 */
API_AVAILABLE(macos(10.7), ios(4.3))
DISPATCH_EXPORT DISPATCH_NONNULL1 DISPATCH_NONNULL3 DISPATCH_NOTHROW
void
dispatch_barrier_sync_f(dispatch_queue_t queue,
		void *_Nullable context, dispatch_function_t work);

/*!
 * @function dispatch_barrier_async_and_wait
 *
 * @abstract
 * Submits a block for synchronous execution on a dispatch queue.
 * 提交一个块以在调度队列上同步执行
 *
 * @discussion
 * Submits a block to a dispatch queue like dispatch_async_and_wait(), but marks
 * that block as a barrier (relevant only on DISPATCH_QUEUE_CONCURRENT
 * queues).
 * 将一个块提交到诸如 dispatch_async_and_wait 之类的调度队列中，但将该块标记为屏障（barrier）（仅与 DISPATCH_QUEUE_CONCURRENT 队列相关）。
 *
 * See "Dispatch Barrier API" for a description of the barrier semantics.
 *
 * @param queue
 * The target dispatch queue to which the block is submitted.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param work
 * The application-defined block to invoke on the target queue.
 * The result of passing NULL in this parameter is undefined.
 */
#ifdef __BLOCKS__
API_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
dispatch_barrier_async_and_wait(dispatch_queue_t queue,
		DISPATCH_NOESCAPE dispatch_block_t block);
#endif

/*!
 * @function dispatch_barrier_async_and_wait_f
 *
 * @abstract
 * Submits a function for synchronous execution on a dispatch queue.
 *
 * @discussion
 * Submits a function to a dispatch queue like dispatch_async_and_wait_f(), but
 * marks that function as a barrier (relevant only on DISPATCH_QUEUE_CONCURRENT
 * queues).
 *
 * See "Dispatch Barrier API" for a description of the barrier semantics.
 *
 * @param queue
 * The target dispatch queue to which the function is submitted.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param context
 * The application-defined context parameter to pass to the function.
 *
 * @param work
 * The application-defined function to invoke on the target queue. The first
 * parameter passed to this function is the context provided to
 * dispatch_barrier_async_and_wait_f().
 * The result of passing NULL in this parameter is undefined.
 */
API_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0))
DISPATCH_EXPORT DISPATCH_NONNULL1 DISPATCH_NONNULL3 DISPATCH_NOTHROW
void
dispatch_barrier_async_and_wait_f(dispatch_queue_t queue,
		void *_Nullable context, dispatch_function_t work);

/*!
 * @functiongroup Dispatch queue-specific contexts
 * This API allows different subsystems to associate context to a shared queue
 * without risk of collision and to retrieve that context from blocks executing
 * on that queue or any of its child queues in the target queue hierarchy.
 * 这个API允许不同的子系统将上下文与共享队列关联起来，而不会有冲突的风险，
 * 并且可以从目标队列层次结构中该队列或其任何子队列上执行的块检索上下文。
 */

/*!
 * @function dispatch_queue_set_specific
 *
 * @abstract
 * Associates a subsystem-specific context with a dispatch queue, for a key
 * unique to the subsystem.
 * 将子系统特定的上下文与调度队列相关联，以获得子系统特有的 key（这里 key 参数类型是 const void * （指针指向可以变，但是指向的内容不能通过该指针修改））。
 *
 * @discussion
 * The specified destructor will be invoked with the context on the default
 * priority global concurrent queue when a new context is set for the same key,
 * or after all references to the queue have been released.
 * 当为同一个 key 设置了新的上下文时，或者在释放了对队列的所有引用之后，将使用默认优先级全局并发队列上的上下文调用指定的析构函数。
 *
 * @param queue
 * The dispatch queue to modify.
 * The result of passing NULL in this parameter is undefined.
 * 调度队列进行修改。在此参数中传递 NULL 的结果是不确定的。
 *
 * @param key
 * The key to set the context for, typically a pointer to a static variable
 * specific to the subsystem. Keys are only compared as pointers and never
 * dereferenced. Passing a string constant directly is not recommended.
 * The NULL key is reserved and attempts to set a context for it are ignored.
 * 要为其设置上下文的键，通常是指向特定于子系统的静态变量的指针。 key 只作为指针进行比较，从不取消引用。不建议直接传递字符串常量，保留 NULL 键，并忽略为其设置上下文的尝试。
 *
 * @param context
 * The new subsystem-specific context for the object. This may be NULL.
 * 对象的新的特定于子系统的上下文。这可能为 NULL。
 *
 * @param destructor
 * The destructor function pointer. This may be NULL and is ignored if context
 * is NULL.
 * 析构函数的指针。这可以为 NULL，如果 context 为 NULL，则将其忽略。
 */
API_AVAILABLE(macos(10.7), ios(5.0))
DISPATCH_EXPORT DISPATCH_NONNULL1 DISPATCH_NOTHROW
void
dispatch_queue_set_specific(dispatch_queue_t queue, const void *key,
		void *_Nullable context, dispatch_function_t _Nullable destructor);

/*!
 * @function dispatch_queue_get_specific
 *
 * @abstract
 * Returns the subsystem-specific context associated with a dispatch queue, for
 * a key unique to the subsystem.
 * 返回与调度队列相关联的子系统特定上下文，用于子系统唯一的键。
 *
 * @discussion
 * Returns the context for the specified key if it has been set on the specified
 * queue.
 * 如果已在指定队列上设置了指定键，则返回该键的上下文。
 *
 * @param queue
 * The dispatch queue to query.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param key
 * The key to get the context for, typically a pointer to a static variable
 * specific to the subsystem. Keys are only compared as pointers and never
 * dereferenced. Passing a string constant directly is not recommended.
 * 取上下文的键，通常是指向特定于子系统的静态变量的指针。key 仅作为指针进行比较，而不会取消引用，不建议直接传递字符串常量。
 *
 * @result
 * The context for the specified key or NULL if no context was found.
 * 指定键的上下文；如果未找到上下文，则为 NULL。
 */
API_AVAILABLE(macos(10.7), ios(5.0))
DISPATCH_EXPORT DISPATCH_NONNULL1 DISPATCH_PURE DISPATCH_WARN_RESULT
DISPATCH_NOTHROW
void *_Nullable
dispatch_queue_get_specific(dispatch_queue_t queue, const void *key);

/*!
 * @function dispatch_get_specific
 *
 * @abstract
 * Returns the current subsystem-specific context for a key unique to the
 * subsystem.
 * 返回子系统唯一的 key 的当前特定于子系统的上下文。
 *
 * @discussion
 * When called from a block executing on a queue, returns the context for the
 * specified key if it has been set on the queue, otherwise returns the result
 * of dispatch_get_specific() executed on the queue's target queue or NULL
 * if the current queue is a global concurrent queue.
 * 从队列上执行的块调用时，如果指定键已在队列上设置，则返回该键的上下文；否则，返回在队列的目标队列上执行的 dispatch_get_specific 的结果；如果当前队列是全局并发队列，则返回 NULL。
 *
 * @param key
 * The key to get the context for, typically a pointer to a static variable
 * specific to the subsystem. Keys are only compared as pointers and never
 * dereferenced. Passing a string constant directly is not recommended.
 * 获取上下文的键，通常是指向特定于子系统的静态变量的指针。key 仅作为指针进行比较，而不会取消引用，不建议直接传递字符串常量。
 *
 * @result
 * The context for the specified key or NULL if no context was found.
 * 指定 key 的上下文；如果未找到上下文，则为 NULL。
 */
API_AVAILABLE(macos(10.7), ios(5.0))
DISPATCH_EXPORT DISPATCH_PURE DISPATCH_WARN_RESULT DISPATCH_NOTHROW
void *_Nullable
dispatch_get_specific(const void *key);

/*!
 * @functiongroup Dispatch assertion API
 *
 * This API asserts at runtime that code is executing in (or out of) the context
 * of a given queue. It can be used to check that a block accessing a resource
 * does so from the proper queue protecting the resource. It also can be used
 * to verify that a block that could cause a deadlock if run on a given queue
 * never executes on that queue.
 * Dispatch assertion API 在运行时断言代码正在给定队列的上下文中执行（或从其执行）。
 * 它可用于从保护资源的适当队列中检查访问资源的块是否这样做。
 * 它还可以用于验证如果在给定队列上运行可能导致死锁的块永远不会在该队列上执行。
 */

/*!
 * @function dispatch_assert_queue
 *
 * @abstract
 * Verifies that the current block is executing on a given dispatch queue.
 *  验证当前块是否在给定的调度队列上执行。
 *
 * @discussion
 * Some code expects to be run on a specific dispatch queue. This function
 * verifies that that expectation is true.
 * 某些代码希望在特定的调度队列上运行，此函数验证该期望为真。
 *
 * If the currently executing block was submitted to the specified queue or to
 * any queue targeting it (see dispatch_set_target_queue()), this function
 * returns.
 * 如果当前正在执行的块已提交给​​指定队列或任何以它为目标的队列（参阅 dispatch_set_target_queue），则此函数返回。
 *
 * If the currently executing block was submitted with a synchronous API
 * (dispatch_sync(), dispatch_barrier_sync(), ...), the context of the
 * submitting block is also evaluated (recursively).
 * If a synchronously submitting block is found that was itself submitted to
 * the specified queue or to any queue targeting it, this function returns.
 * 如果当前执行的块是使用同步 API 提交的（dispatch_sync，dispatch_barrier_sync 等），则也会（递归地）评估提交块的上下文。如果发现同步提交的块本身已提交到指定队列或任何以它为目标的队列，则此函数返回。
 *
 * Otherwise this function asserts: it logs an explanation to the system log and
 * terminates the application.
 * 否则，此函数将声明：将解释记录到系统日志并终止应用程序。
 *
 * Passing the result of dispatch_get_main_queue() to this function verifies
 * that the current block was submitted to the main queue, or to a queue
 * targeting it, or is running on the main thread (in any context).
 * 将 dispatch_get_main_queue 的结果传递给此函数可验证当前块是否已提交到主队列或提交给它的队列，或者是否正在主线程上运行（在任何上下文中）。
 *
 * When dispatch_assert_queue() is called outside of the context of a
 * submitted block (for example from the context of a thread created manually
 * with pthread_create()) then this function will also assert and terminate
 * the application.
 * 当在提交的块的上下文之外（例如，从使用 pthread_create 手动创建的线程的上下文中）调用 dispatch_assert_queue 时，此函数还将声明并终止应用程序。
 *
 * The variant dispatch_assert_queue_debug() is compiled out when the
 * preprocessor macro NDEBUG is defined. (See also assert(3)).
 *
 * @param queue
 * The dispatch queue that the current block is expected to run on.
 * The result of passing NULL in this parameter is undefined.
 * 当前块应在其上运行的调度队列。在此参数中传递 NULL 的结果是不确定的。
 */
API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
DISPATCH_EXPORT DISPATCH_NONNULL1
void
dispatch_assert_queue(dispatch_queue_t queue)
		DISPATCH_ALIAS_V2(dispatch_assert_queue);

/*!
 * @function dispatch_assert_queue_barrier
 *
 * @abstract
 * Verifies that the current block is executing on a given dispatch queue,
 * and that the block acts as a barrier on that queue.
 * dispatch_assert_queue_barrier 验证当前块是否在给定的调度队列上执行，并且该块充当该队列上的屏障（barrier）。
 *
 * @discussion
 * This behaves exactly like dispatch_assert_queue(), with the additional check
 * that the current block acts as a barrier on the specified queue, which is
 * always true if the specified queue is serial (see DISPATCH_BLOCK_BARRIER or
 * dispatch_barrier_async() for details).
 * 行为与 dispatch_assert_queue 完全一样，另外还要检查当前块是否充当指定队列上的屏障，如果指定队列是串行的，则始终为 true（参见 DISPATCH_BLOCK_BARRIER 或 dispatch_barrier_async）。
 *
 * The variant dispatch_assert_queue_barrier_debug() is compiled out when the
 * preprocessor macro NDEBUG is defined. (See also assert()).
 *
 * @param queue
 * The dispatch queue that the current block is expected to run as a barrier on.
 * The result of passing NULL in this parameter is undefined.
 * 当前块应作为屏障运行的调度队列。在此参数中传递 NULL 的结果是不确定的。
 */
API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
DISPATCH_EXPORT DISPATCH_NONNULL1
void
dispatch_assert_queue_barrier(dispatch_queue_t queue);

/*!
 * @function dispatch_assert_queue_not
 *
 * @abstract
 * Verifies that the current block is not executing on a given dispatch queue.
 * dispatch_assert_queue_not
 *
 * @discussion
 * This function is the equivalent of dispatch_assert_queue() with the test for
 * equality inverted. That means that it will terminate the application when
 * dispatch_assert_queue() would return, and vice-versa. See discussion there.
 * 等效于 dispatch_assert_queue，但相等性测试却相反。这意味着它将在 dispatch_assert_queue 返回时终止应用程序，反之亦然。
 *
 * The variant dispatch_assert_queue_not_debug() is compiled out when the
 * preprocessor macro NDEBUG is defined. (See also assert(3)).
 *
 * @param queue
 * The dispatch queue that the current block is expected not to run on.
 * The result of passing NULL in this parameter is undefined.
 * 当前块不应在其上运行的调度队列。在此参数中传递 NULL 的结果是不确定的。
 */
API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
DISPATCH_EXPORT DISPATCH_NONNULL1
void
dispatch_assert_queue_not(dispatch_queue_t queue)
		DISPATCH_ALIAS_V2(dispatch_assert_queue_not);

#ifdef NDEBUG
#define dispatch_assert_queue_debug(q) ((void)(0 && (q)))
#define dispatch_assert_queue_barrier_debug(q) ((void)(0 && (q)))
#define dispatch_assert_queue_not_debug(q) ((void)(0 && (q)))
#else
#define dispatch_assert_queue_debug(q) dispatch_assert_queue(q)
#define dispatch_assert_queue_barrier_debug(q) dispatch_assert_queue_barrier(q)
#define dispatch_assert_queue_not_debug(q) dispatch_assert_queue_not(q)
#endif

__END_DECLS

DISPATCH_ASSUME_NONNULL_END

#endif
