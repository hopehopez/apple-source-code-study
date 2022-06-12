/*
 * Copyright (c) 2008-2012 Apple Inc. All rights reserved.
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

#ifndef __DISPATCH_OBJECT__
#define __DISPATCH_OBJECT__

#ifndef __DISPATCH_INDIRECT__
#error "Please #include <dispatch/dispatch.h> instead of this file directly."
#include <dispatch/base.h> // for HeaderDoc
#endif

#if __has_include(<sys/qos.h>)
#include <sys/qos.h>
#endif

DISPATCH_ASSUME_NONNULL_BEGIN

/*!
 * @typedef dispatch_object_t
 *
 * @abstract
 * Abstract base type for all dispatch objects.
 * The details of the type definition are language-specific.
 * dispatch_object_t 是所有调度对象（dispatch objects）的抽象基类型，且 dispatch_object_t 的具体定义在特定语言（Swift/Objective-C/C）下不同。
 *
 * @discussion
 * Dispatch objects are reference counted via calls to dispatch_retain() and
 * dispatch_release().
 * 调度对象通过调用 dispatch_retain 和 dispatch_release 进行引用计数管理。
 */

#if OS_OBJECT_USE_OBJC
/*
 * By default, dispatch objects are declared as Objective-C types when building
 * with an Objective-C compiler. This allows them to participate in ARC, in RR
 * management by the Blocks runtime and in leaks checking by the static
 * analyzer, and enables them to be added to Cocoa collections.
 * See <os/object.h> for details.
 */
/*
 * 默认情况下，使用 Objective-C 编译器进行构建时，dispatch objects 被声明为 Objective-C 类型（NSObject）。
 * 这使他们可以参与 ARC，通过 Blocks 运行时参与 RR（retain/release）管理以及通过静态分析器参与泄漏检查，
 * 并将它们添加到 Cocoa 集合（NSMutableArray、NSMutableDictionary...）中。
 
 */
OS_OBJECT_DECL_CLASS(dispatch_object);

#if OS_OBJECT_SWIFT3

#define DISPATCH_DECL(name) OS_OBJECT_DECL_SUBCLASS_SWIFT(name, dispatch_object)
#define DISPATCH_DECL_SUBCLASS(name, base) OS_Odispatch_block_tBJECT_DECL_SUBCLASS_SWIFT(name, base)

#else // OS_OBJECT_SWIFT3

///默认使用 dispatch_object 作为继承的父类
#define DISPATCH_DECL(name) OS_OBJECT_DECL_SUBCLASS(name, dispatch_object)

///自行指定父类，并且针对不同的语言环境作了不同的定义。
#define DISPATCH_DECL_SUBCLASS(name, base) OS_OBJECT_DECL_SUBCLASS(name, base)

///这个函数的目的是把传进来的 object 的首地址取出来，然后强转为 void *。（把结构体的首元素的首地址转换为 isa 使用吗？）
DISPATCH_INLINE DISPATCH_ALWAYS_INLINE DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
_dispatch_object_validate(dispatch_object_t object)
{
	void *isa = *(void *volatile*)(OS_OBJECT_BRIDGE void*)object;
	(void)isa;
}
#endif // OS_OBJECT_USE_OBJC

#define DISPATCH_GLOBAL_OBJECT(type, object) ((OS_OBJECT_BRIDGE type)&(object))
#define DISPATCH_RETURNS_RETAINED OS_OBJECT_RETURNS_RETAINED

#elif defined(__cplusplus) && !defined(__DISPATCH_BUILDING_DISPATCH__)
// c++
/*
 * Dispatch objects are NOT C++ objects. Nevertheless, we can at least keep C++
 * aware of type compatibility.
 */
typedef struct dispatch_object_s {
private:
	dispatch_object_s();
	~dispatch_object_s();
	dispatch_object_s(const dispatch_object_s &);
	void operator=(const dispatch_object_s &);
} *dispatch_object_t;
#define DISPATCH_DECL(name) \
		typedef struct name##_s : public dispatch_object_s {} *name##_t
#define DISPATCH_DECL_SUBCLASS(name, base) \
		typedef struct name##_s : public base##_s {} *name##_t
#define DISPATCH_GLOBAL_OBJECT(type, object) (static_cast<type>(&(object)))
#define DISPATCH_RETURNS_RETAINED
// c++ end

#else /* Plain C */
#ifndef __DISPATCH_BUILDING_DISPATCH__
typedef union {
	struct _os_object_s *_os_obj;
	struct dispatch_object_s *_do;
	struct dispatch_queue_s *_dq;
	struct dispatch_queue_attr_s *_dqa;
	struct dispatch_group_s *_dg;
	struct dispatch_source_s *_ds;
	struct dispatch_channel_s *_dch;
	struct dispatch_mach_s *_dm;
	struct dispatch_mach_msg_s *_dmsg;
	struct dispatch_semaphore_s *_dsema;
	struct dispatch_data_s *_ddata;
	struct dispatch_io_s *_dchannel;
} dispatch_object_t DISPATCH_TRANSPARENT_UNION;
#endif // !__DISPATCH_BUILDING_DISPATCH__
#define DISPATCH_DECL(name) typedef struct name##_s *name##_t
#define DISPATCH_DECL_SUBCLASS(name, base) typedef base##_t name##_t
#define DISPATCH_GLOBAL_OBJECT(type, object) ((type)&(object))
#define DISPATCH_RETURNS_RETAINED
/* Plain C end */
#endif

#if OS_OBJECT_SWIFT3 && OS_OBJECT_USE_OBJC
#define DISPATCH_SOURCE_TYPE_DECL(name) \
		DISPATCH_EXPORT struct dispatch_source_type_s \
				_dispatch_source_type_##name; \
		OS_OBJECT_DECL_PROTOCOL(dispatch_source_##name, <OS_dispatch_source>); \
		OS_OBJECT_CLASS_IMPLEMENTS_PROTOCOL( \
				dispatch_source, dispatch_source_##name)
#define DISPATCH_SOURCE_DECL(name) \
		DISPATCH_DECL(name); \
		OS_OBJECT_DECL_PROTOCOL(name, <NSObject>); \
		OS_OBJECT_CLASS_IMPLEMENTS_PROTOCOL(name, name)
#ifndef DISPATCH_DATA_DECL
#define DISPATCH_DATA_DECL(name) OS_OBJECT_DECL_SWIFT(name)
#endif // DISPATCH_DATA_DECL
#else
#define DISPATCH_SOURCE_DECL(name) \
		DISPATCH_DECL(name);
#define DISPATCH_DATA_DECL(name) DISPATCH_DECL(name)
#define DISPATCH_SOURCE_TYPE_DECL(name) \
		DISPATCH_EXPORT const struct dispatch_source_type_s \
		_dispatch_source_type_##name
#endif

#ifdef __BLOCKS__
/*!
 * @typedef dispatch_block_t
 *
 * @abstract
 * The type of blocks submitted to dispatch queues, which take no arguments
 * and have no return value.
 *
 * @discussion
 * When not building with Objective-C ARC, a block object allocated on or
 * copied to the heap must be released with a -[release] message or the
 * Block_release() function.
 *
 * The declaration of a block literal allocates storage on the stack.
 * Therefore, this is an invalid construct:
 * <code>
 * dispatch_block_t block;
 * if (x) {
 *     block = ^{ printf("true\n"); };
 * } else {
 *     block = ^{ printf("false\n"); };
 * }
 * block(); // unsafe!!!
 * </code>
 *
 * What is happening behind the scenes:
 * <code>
 * if (x) {
 *     struct Block __tmp_1 = ...; // setup details
 *     block = &__tmp_1;
 * } else {
 *     struct Block __tmp_2 = ...; // setup details
 *     block = &__tmp_2;
 * }
 * </code>
 *
 * As the example demonstrates, the address of a stack variable is escaping the
 * scope in which it is allocated. That is a classic C bug.
 *
 * Instead, the block literal must be copied to the heap with the Block_copy()
 * function or by sending it a -[copy] message.
 */
typedef void (^dispatch_block_t)(void);
#endif // __BLOCKS__

__BEGIN_DECLS

/*!
 * @typedef dispatch_qos_class_t
 * Alias for qos_class_t type.
 */
#if __has_include(<sys/qos.h>)
typedef qos_class_t dispatch_qos_class_t;
#else
typedef unsigned int dispatch_qos_class_t;
#endif

/*!
 * @function dispatch_retain
 *
 * @abstract
 * Increment the reference count of a dispatch object.
 * 增加调度对象（dispatch object）的引用计数。
 *
 * @discussion
 * Calls to dispatch_retain() must be balanced with calls to
 * dispatch_release().
 * 对 dispatch_retain 的调用必须与对 dispatch_release 的调用保持平衡。
 *
 * @param object
 * The object to retain.
 * The result of passing NULL in this parameter is undefined.
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
DISPATCH_SWIFT_UNAVAILABLE("Can't be used with ARC")
void
dispatch_retain(dispatch_object_t object);
#if OS_OBJECT_USE_OBJC_RETAIN_RELEASE
#undef dispatch_retain
#define dispatch_retain(object) \
		__extension__({ dispatch_object_t _o = (object); \
		_dispatch_object_validate(_o); (void)[_o retain]; })
#endif

/*!
 * @function dispatch_release
 *
 * @abstract
 * Decrement the reference count of a dispatch object.
 * dispatch_release 减少调度对象的引用计数。
 *
 * @discussion
 * A dispatch object is asynchronously deallocated once all references are
 * released (i.e. the reference count becomes zero). The system does not
 * guarantee that a given client is the last or only reference to a given
 * object.
 * 一旦释放了所有引用（即引用计数变为零），就会异步释放 dispatch object。系统不保证给定的客户端（client）是对给定对象的最后或唯一引用。
 *
 * @param object
 * The object to release.
 * The result of passing NULL in this parameter is undefined.
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
DISPATCH_SWIFT_UNAVAILABLE("Can't be used with ARC")
void
dispatch_release(dispatch_object_t object);
#if OS_OBJECT_USE_OBJC_RETAIN_RELEASE
#undef dispatch_release
#define dispatch_release(object) \
		__extension__({ dispatch_object_t _o = (object); \
		_dispatch_object_validate(_o); [_o release]; })
#endif

/*!
 * @function dispatch_get_context
 *
 * @abstract
 * Returns the application defined context of the object.
 * 返回应用程序定义的对象的上下文。
 *
 * @param object
 * The result of passing NULL in this parameter is undefined.
 * object 在此参数中传递 NULL 的结果是未定义的。
 *
 * @result
 * The context of the object; may be NULL.
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_PURE DISPATCH_WARN_RESULT
DISPATCH_NOTHROW
void *_Nullable
dispatch_get_context(dispatch_object_t object);

/*!
 * @function dispatch_set_context
 *
 * @abstract
 * Associates an application defined context with the object.
 * 将应用程序定义的上下文与对象相关联
 *
 * @param object
 * The result of passing NULL in this parameter is undefined.
 *
 * @param context
 * The new client defined context for the object. This may be NULL.
 *
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NOTHROW
void
dispatch_set_context(dispatch_object_t object, void *_Nullable context);

/*!
 * @function dispatch_set_finalizer_f
 *
 * @abstract
 * Set the finalizer function for a dispatch object.
 * 为 dispatch object 设置终结（finalizer）函数。
 *
 * @param object
 * The dispatch object to modify.
 * The result of passing NULL in this parameter is undefined.
 * 要修改的 dispatch object。在此参数中传递 NULL 的结果是未定义的
 *
 * @param finalizer
 * The finalizer function pointer.
 * 终结（finalizer）函数的指针。
 *
 * @discussion
 * A dispatch object's finalizer will be invoked on the object's target queue
 * after all references to the object have been released. This finalizer may be
 * used by the application to release any resources associated with the object,
 * such as freeing the object's context.
 * The context parameter passed to the finalizer function is the current
 * context of the dispatch object at the time the finalizer call is made.
 * 在释放对对象的所有引用之后，将在对象的目标队列上调用调度对象的终结函数。应用程序可以使用此终结函数来释放与对象关联的任何资源，例如释放对象的上下文。
 * 传递给终结函数的 context 参数是在进行终结函数调用时调度对象的当前上下文。
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NOTHROW
void
dispatch_set_finalizer_f(dispatch_object_t object,
		dispatch_function_t _Nullable finalizer);

/*!
 * @function dispatch_activate
 *
 * @abstract
 * Activates the specified dispatch object.
 * 激活指定的调度对象。
 *
 * @discussion
 * Dispatch objects such as queues and sources may be created in an inactive
 * state. Objects in this state have to be activated before any blocks
 * associated with them will be invoked.
 * 调度对象（例如队列（queues）和源（sources））可以在非活动状态下创建，
 * 必须先激活处于这种状态的对象，然后才能调用与它们关联的任何块（blocks）。
 *
 * The target queue of inactive objects can be changed using
 * dispatch_set_target_queue(). Change of target queue is no longer permitted
 * once an initially inactive object has been activated.
 * 可以使用 dispatch_set_target_queue 更改非活动对象的目标队列，一旦最初不活动的对象被激活，就不再允许更改目标队列。
 *
 * Calling dispatch_activate() on an active object has no effect.
 * Releasing the last reference count on an inactive object is undefined.
 * 在活动对象上调用 dispatch_activate 无效，释放非活动对象上的最后一个引用计数是未定义的。
 *
 * @param object
 * The object to be activated.
 * The result of passing NULL in this parameter is undefined.
 * 要激活的对象。在此参数中传递 NULL 的结果是未定义的。
 */
API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
dispatch_activate(dispatch_object_t object);

/*!
 * @function dispatch_suspend
 *
 * @abstract
 * Suspends the invocation of blocks on a dispatch object.
 * 挂起调度对象上的blocks。
 *
 * @discussion
 * A suspended object will not invoke any blocks associated with it. The
 * suspension of an object will occur after any running block associated with
 * the object completes.
 * 挂起的对象将不会调用与其关联的任何blocks。与该对象关联的任何运行块（running block）完成之后，将发生对象的挂起。
 *
 * Calls to dispatch_suspend() must be balanced with calls
 * to dispatch_resume().
 * 对 dispatch_suspend() 的调用必须与对 dispatch_resume 的调用保持平衡。
 *
 * @param object
 * The object to be suspended.
 * The result of passing NULL in this parameter is undefined.
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
dispatch_suspend(dispatch_object_t object);

/*!
 * @function dispatch_resume
 *
 * @abstract
 * Resumes the invocation of blocks on a dispatch object.
 * 恢复调度对象上块的调用
 *
 * @discussion
 * Dispatch objects can be suspended with dispatch_suspend(), which increments
 * an internal suspension count. dispatch_resume() is the inverse operation,
 * and consumes suspension counts. When the last suspension count is consumed,
 * blocks associated with the object will be invoked again.
 * 可以使用 dispatch_suspend 挂起 dispatch objects，这会增加内部挂起计数. dispatch_resume 是逆运算，它消耗暂停计数。当最后一次暂停计数被消耗时，与该对象关联的块将再次被调用。
 *
 * For backward compatibility reasons, dispatch_resume() on an inactive and not
 * otherwise suspended dispatch source object has the same effect as calling
 * dispatch_activate(). For new code, using dispatch_activate() is preferred.
 * 出于向后兼容性的原因，在非活动且未暂停的调度源对象上的 dispatch_resume 与调用 dispatch_activate 具有相同的效果。对于新代码，首选使用 dispatch_activate。
 *
 * If the specified object has zero suspension count and is not an inactive
 * source, this function will result in an assertion and the process being
 * terminated.
 * 如果指定的对象的挂起计数为零并且不是非活动源，则此函数将导致断言并终止进程。
 *
 * @param object
 * The object to be resumed.
 * The result of passing NULL in this parameter is undefined.
 */
API_AVAILABLE(macos(10.6), ios(4.0))
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
dispatch_resume(dispatch_object_t object);

/*!
 * @function dispatch_set_qos_class_floor
 *
 * @abstract
 * Sets the QOS class floor on a dispatch queue, source or workloop.
 *
 * @discussion
 * The QOS class of workitems submitted to this object asynchronously will be
 * elevated to at least the specified QOS class floor. The QOS of the workitem
 * will be used if higher than the floor even when the workitem has been created
 * without "ENFORCE" semantics.
 * 异步提交给该对象的工作项的 QOS 等级将至少提升到指定的 QOS 等级。 即使在没有“ENFORCE”语义的情况下创建了工作项，也将使用工作项的 QOS 如果高于地板。
 *
 * Setting the QOS class floor is equivalent to the QOS effects of configuring
 * a queue whose target queue has a QoS class set to the same value.
 * 设置 QOS 等级下限等效于配置目标队列的 QoS 等级设置为相同值的队列的 QOS 效果。
 *
 * @param object
 * A dispatch queue, workloop, or source to configure.
 * The object must be inactive.
 *
 * Passing another object type or an object that has been activated is undefined
 * and will cause the process to be terminated.
 *
 * @param qos_class
 * A QOS class value:
 *  - QOS_CLASS_USER_INTERACTIVE
 *  - QOS_CLASS_USER_INITIATED
 *  - QOS_CLASS_DEFAULT
 *  - QOS_CLASS_UTILITY
 *  - QOS_CLASS_BACKGROUND
 * Passing any other value is undefined.
 *
 * @param relative_priority
 * A relative priority within the QOS class. This value is a negative
 * offset from the maximum supported scheduler priority for the given class.
 * Passing a value greater than zero or less than QOS_MIN_RELATIVE_PRIORITY
 * is undefined.
 */
API_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0))
DISPATCH_EXPORT DISPATCH_NOTHROW
void
dispatch_set_qos_class_floor(dispatch_object_t object,
		dispatch_qos_class_t qos_class, int relative_priority);

#ifdef __BLOCKS__
/*!
 * @function dispatch_wait
 *
 * @abstract
 * Wait synchronously for an object or until the specified timeout has elapsed.
 * 同步等待某个对象，或直到指定的超时时间已过。
 *
 * @discussion
 * Type-generic macro that maps to dispatch_block_wait, dispatch_group_wait or
 * dispatch_semaphore_wait, depending on the type of the first argument.
 * See documentation for these functions for more details.
 * This function is unavailable for any other object type.
 * 类型通用宏，根据第一个参数的类型，映射到 dispatch_block_wait，dispatch_group_wait 或 dispatch_semaphore_wait
 * 此功能不适用于任何其他对象类型。
 *
 * @param object
 * The object to wait on.
 * The result of passing NULL in this parameter is undefined.
 * 要等待的对象。在此参数中传递 NULL 的结果是未定义的。
 *
 * @param timeout
 * When to timeout (see dispatch_time). As a convenience, there are the
 * DISPATCH_TIME_NOW and DISPATCH_TIME_FOREVER constants.
 * 何时超时，为方便起见，有 DISPATCH_TIME_NOW 和 DISPATCH_TIME_FOREVER 常量。
 *
 * @result
 * Returns zero on success or non-zero on error (i.e. timed out).
 */
DISPATCH_UNAVAILABLE
DISPATCH_EXPORT DISPATCH_NONNULL1 DISPATCH_NOTHROW
intptr_t
dispatch_wait(void *object, dispatch_time_t timeout);
#if __has_extension(c_generic_selections)
#define dispatch_wait(object, timeout) \
		_Generic((object), \
			dispatch_block_t:dispatch_block_wait, \
			dispatch_group_t:dispatch_group_wait, \
			dispatch_semaphore_t:dispatch_semaphore_wait \
		)((object),(timeout))
#endif

/*!
 * @function dispatch_notify
 *
 * @abstract
 * Schedule a notification block to be submitted to a queue when the execution
 * of a specified object has completed.
 * 安排在完成指定对象的执行后将通知块提交给队列。
 *
 * @discussion
 * Type-generic macro that maps to dispatch_block_notify or
 * dispatch_group_notify, depending on the type of the first argument.
 * See documentation for these functions for more details.
 * This function is unavailable for any other object type.
  根据第一个参数的类型，映射到 dispatch_block_notify 或 dispatch_group_notify 的类型通用宏，此功能不适用于任何其他对象类型。
 *
 * @param object
 * The object to observe.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param queue
 * The queue to which the supplied notification block will be submitted when
 * the observed object completes.
 *
 * @param notification_block
 * The block to submit when the observed object completes.
 */
DISPATCH_UNAVAILABLE
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
dispatch_notify(void *object, dispatch_object_t queue,
		dispatch_block_t notification_block);
#if __has_extension(c_generic_selections)
#define dispatch_notify(object, queue, notification_block) \
		_Generic((object), \
			dispatch_block_t:dispatch_block_notify, \
			dispatch_group_t:dispatch_group_notify \
		)((object),(queue), (notification_block))
#endif

/*!
 * @function dispatch_cancel
 *
 * @abstract
 * Cancel the specified object.
 * 取消指定的对象。
 *
 * @discussion
 * Type-generic macro that maps to dispatch_block_cancel or
 * dispatch_source_cancel, depending on the type of the first argument.
 * See documentation for these functions for more details.
 * This function is unavailable for any other object type.
 * 根据第一个参数的类型映射到 dispatch_block_cancel 或 dispatch_source_cancel 的类型通用宏。此功能不适用于任何其他对象类型。
 *
 * @param object
 * The object to cancel.
 * The result of passing NULL in this parameter is undefined.
 */
DISPATCH_UNAVAILABLE
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
dispatch_cancel(void *object);
#if __has_extension(c_generic_selections)
#define dispatch_cancel(object) \
		_Generic((object), \
			dispatch_block_t:dispatch_block_cancel, \
			dispatch_source_t:dispatch_source_cancel \
		)((object))
#endif

/*!
 * @function dispatch_testcancel
 *
 * @abstract
 * Test whether the specified object has been canceled
 * 测试指定对象是否已被取消。
 *
 * @discussion
 * Type-generic macro that maps to dispatch_block_testcancel or
 * dispatch_source_testcancel, depending on the type of the first argument.
 * See documentation for these functions for more details.
 * This function is unavailable for any other object type.
 * 类型通用的宏，它映射到 dispatch_block_testcancel 或 dispatch_source_testcancel，具体取决于第一个参数的类型。此功能不适用于任何其他对象类型。
 *
 * @param object
 * The object to test.
 * The result of passing NULL in this parameter is undefined.
 * 要测试的对象
 * @result
 * Non-zero if canceled and zero if not canceled.
 */
DISPATCH_UNAVAILABLE
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_WARN_RESULT DISPATCH_PURE
DISPATCH_NOTHROW
intptr_t
dispatch_testcancel(void *object);
#if __has_extension(c_generic_selections)
#define dispatch_testcancel(object) \
		_Generic((object), \
			dispatch_block_t:dispatch_block_testcancel, \
			dispatch_source_t:dispatch_source_testcancel \
		)((object))
#endif
#endif // __BLOCKS__

/*!
 * @function dispatch_debug
 *
 * @abstract
 * Programmatically log debug information about a dispatch object.
 * 以编程方式记录有关调度对象的调试信息。
 *
 * @discussion
 * Programmatically log debug information about a dispatch object. By default,
 * the log output is sent to syslog at notice level. In the debug version of
 * the library, the log output is sent to a file in /var/tmp.
 * The log output destination can be configured via the LIBDISPATCH_LOG
 * environment variable, valid values are: YES, NO, syslog, stderr, file.
 * 以编程方式记录有关调度对象的调试信息。默认情况下，日志输出以通知级别发送到 syslog。在库的调试版本中，日志输出发送到 /var/tmp 中的文件。
 * 可以通过 LIBDISPATCH_LOG 环境变量配置日志输出目标。
 *
 * This function is deprecated and will be removed in a future release.
 * Objective-C callers may use -debugDescription instead.
 * 不建议使用此功能，以后的版本中将删除该功能。 Objective-C 调用者可以改用 debugDescription
 *
 * @param object
 * The object to introspect.
 *
 * @param message
 * The message to log above and beyond the introspection.
 */
API_DEPRECATED("unsupported interface", macos(10.6,10.9), ios(4.0,6.0))
DISPATCH_EXPORT DISPATCH_NONNULL2 DISPATCH_NOTHROW DISPATCH_COLD
__attribute__((__format__(printf,2,3)))
void
dispatch_debug(dispatch_object_t object, const char *message, ...);

API_DEPRECATED("unsupported interface", macos(10.6,10.9), ios(4.0,6.0))
DISPATCH_EXPORT DISPATCH_NONNULL2 DISPATCH_NOTHROW DISPATCH_COLD
__attribute__((__format__(printf,2,0)))
void
dispatch_debugv(dispatch_object_t object, const char *message, va_list ap);

__END_DECLS

DISPATCH_ASSUME_NONNULL_END

#endif
