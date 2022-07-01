/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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

/*	CFRunLoop.c
    Copyright (c) 1998-2014, Apple Inc. All rights reserved.
    Responsibility: Tony Parker
*/

#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFBag.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPreferences.h>
#include "CFInternal.h"
#include <math.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <dispatch/dispatch.h>

#if DEPLOYMENT_TARGET_WINDOWS
#include <typeinfo.h>
#endif
#include <checkint.h>

#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
#include <sys/param.h>
#include <dispatch/private.h>
#include <CoreFoundation/CFUserNotification.h>
#include <mach/mach.h>
#include <mach/clock_types.h>
#include <mach/clock.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread/private.h>
#include <os/voucher_private.h>
extern mach_port_t _dispatch_get_main_queue_port_4CF(void);
extern void _dispatch_main_queue_callback_4CF(mach_msg_header_t *msg);
#elif DEPLOYMENT_TARGET_WINDOWS
#include <process.h>
DISPATCH_EXPORT HANDLE _dispatch_get_main_queue_handle_4CF(void);
DISPATCH_EXPORT void _dispatch_main_queue_callback_4CF(void);

#define MACH_PORT_NULL 0
#define mach_port_name_t HANDLE
#define mach_port_t HANDLE
#define _dispatch_get_main_queue_port_4CF _dispatch_get_main_queue_handle_4CF
#define _dispatch_main_queue_callback_4CF(x) _dispatch_main_queue_callback_4CF()

#define AbsoluteTime LARGE_INTEGER

#endif

#if DEPLOYMENT_TARGET_WINDOWS || DEPLOYMENT_TARGET_IPHONESIMULATOR
CF_EXPORT pthread_t _CF_pthread_main_thread_np(void);
#define pthread_main_thread_np() _CF_pthread_main_thread_np()
#endif

#include <Block.h>
#include <Block_private.h>

#if DEPLOYMENT_TARGET_MACOSX
#define USE_DISPATCH_SOURCE_FOR_TIMERS 1
#define USE_MK_TIMER_TOO 1
#else
#define USE_DISPATCH_SOURCE_FOR_TIMERS 0
#define USE_MK_TIMER_TOO 1
#endif

static int _LogCFRunLoop = 0;
static void _runLoopTimerWithBlockContext(CFRunLoopTimerRef timer, void *opaqueBlock);

// for conservative arithmetic safety, such that (TIMER_DATE_LIMIT + TIMER_INTERVAL_LIMIT + kCFAbsoluteTimeIntervalSince1970) * 10^9 < 2^63
#define TIMER_DATE_LIMIT 4039289856.0
#define TIMER_INTERVAL_LIMIT 504911232.0

#define HANDLE_DISPATCH_ON_BASE_INVOCATION_ONLY 0

#define CRASH(string, errcode)               \
    do                                       \
    {                                        \
        char msg[256];                       \
        snprintf(msg, 256, string, errcode); \
        CRSetCrashLogMessage(msg);           \
        HALT;                                \
    } while (0)

#if DEPLOYMENT_TARGET_WINDOWS

static pthread_t kNilPthreadT = {nil, nil};
#define pthreadPointer(a) a.p
typedef int kern_return_t;
#define KERN_SUCCESS 0

#else

static pthread_t kNilPthreadT = (pthread_t)0;
#define pthreadPointer(a) a
#define lockCount(a) a
#endif

#pragma mark -

#define CF_RUN_LOOP_PROBES 0

#if CF_RUN_LOOP_PROBES
#include "CFRunLoopProbes.h"
#else
#define CFRUNLOOP_NEXT_TIMER_ARMED(arg0) \
    do                                   \
    {                                    \
    } while (0)
#define CFRUNLOOP_NEXT_TIMER_ARMED_ENABLED() (0)
#define CFRUNLOOP_POLL() \
    do                   \
    {                    \
    } while (0)
#define CFRUNLOOP_POLL_ENABLED() (0)
#define CFRUNLOOP_SLEEP() \
    do                    \
    {                     \
    } while (0)
#define CFRUNLOOP_SLEEP_ENABLED() (0)
#define CFRUNLOOP_SOURCE_FIRED(arg0, arg1, arg2) \
    do                                           \
    {                                            \
    } while (0)
#define CFRUNLOOP_SOURCE_FIRED_ENABLED() (0)
#define CFRUNLOOP_TIMER_CREATED(arg0, arg1, arg2, arg3, arg4, arg5, arg6) \
    do                                                                    \
    {                                                                     \
    } while (0)
#define CFRUNLOOP_TIMER_CREATED_ENABLED() (0)
#define CFRUNLOOP_TIMER_FIRED(arg0, arg1, arg2, arg3, arg4) \
    do                                                      \
    {                                                       \
    } while (0)
#define CFRUNLOOP_TIMER_FIRED_ENABLED() (0)
#define CFRUNLOOP_TIMER_RESCHEDULED(arg0, arg1, arg2, arg3, arg4, arg5) \
    do                                                                  \
    {                                                                   \
    } while (0)
#define CFRUNLOOP_TIMER_RESCHEDULED_ENABLED() (0)
#define CFRUNLOOP_WAKEUP(arg0) \
    do                         \
    {                          \
    } while (0)
#define CFRUNLOOP_WAKEUP_ENABLED() (0)
#define CFRUNLOOP_WAKEUP_FOR_DISPATCH() \
    do                                  \
    {                                   \
    } while (0)
#define CFRUNLOOP_WAKEUP_FOR_DISPATCH_ENABLED() (0)
#define CFRUNLOOP_WAKEUP_FOR_NOTHING() \
    do                                 \
    {                                  \
    } while (0)
#define CFRUNLOOP_WAKEUP_FOR_NOTHING_ENABLED() (0)
#define CFRUNLOOP_WAKEUP_FOR_SOURCE() \
    do                                \
    {                                 \
    } while (0)
#define CFRUNLOOP_WAKEUP_FOR_SOURCE_ENABLED() (0)
#define CFRUNLOOP_WAKEUP_FOR_TIMEOUT() \
    do                                 \
    {                                  \
    } while (0)
#define CFRUNLOOP_WAKEUP_FOR_TIMEOUT_ENABLED() (0)
#define CFRUNLOOP_WAKEUP_FOR_TIMER() \
    do                               \
    {                                \
    } while (0)
#define CFRUNLOOP_WAKEUP_FOR_TIMER_ENABLED() (0)
#define CFRUNLOOP_WAKEUP_FOR_WAKEUP() \
    do                                \
    {                                 \
    } while (0)
#define CFRUNLOOP_WAKEUP_FOR_WAKEUP_ENABLED() (0)
#endif

// In order to reuse most of the code across Mach and Windows v1 RunLoopSources, we define a
// simple abstraction layer spanning Mach ports and Windows HANDLES
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI

CF_PRIVATE uint32_t __CFGetProcessPortCount(void)
{
    ipc_info_space_t info;
    ipc_info_name_array_t table = 0;
    mach_msg_type_number_t tableCount = 0;
    ipc_info_tree_name_array_t tree = 0;
    mach_msg_type_number_t treeCount = 0;

    kern_return_t ret = mach_port_space_info(mach_task_self(), &info, &table, &tableCount, &tree, &treeCount);
    if (ret != KERN_SUCCESS)
    {
        return (uint32_t)0;
    }
    if (table != NULL)
    {
        ret = vm_deallocate(mach_task_self(), (vm_address_t)table, tableCount * sizeof(*table));
    }
    if (tree != NULL)
    {
        ret = vm_deallocate(mach_task_self(), (vm_address_t)tree, treeCount * sizeof(*tree));
    }
    return (uint32_t)tableCount;
}

CF_PRIVATE CFArrayRef __CFStopAllThreads(void)
{
    CFMutableArrayRef suspended_list = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, NULL);
    mach_port_t my_task = mach_task_self();
    mach_port_t my_thread = mach_thread_self();
    thread_act_array_t thr_list = 0;
    mach_msg_type_number_t thr_cnt = 0;

    // really, should loop doing the stopping until no more threads get added to the list N times in a row
    kern_return_t ret = task_threads(my_task, &thr_list, &thr_cnt);
    if (ret == KERN_SUCCESS)
    {
        for (CFIndex idx = 0; idx < thr_cnt; idx++)
        {
            thread_act_t thread = thr_list[idx];
            if (thread == my_thread)
                continue;
            if (CFArrayContainsValue(suspended_list, CFRangeMake(0, CFArrayGetCount(suspended_list)), (const void *)(uintptr_t)thread))
                continue;
            ret = thread_suspend(thread);
            if (ret == KERN_SUCCESS)
            {
                CFArrayAppendValue(suspended_list, (const void *)(uintptr_t)thread);
            }
            else
            {
                mach_port_deallocate(my_task, thread);
            }
        }
        vm_deallocate(my_task, (vm_address_t)thr_list, sizeof(thread_t) * thr_cnt);
    }
    mach_port_deallocate(my_task, my_thread);
    return suspended_list;
}

CF_PRIVATE void __CFRestartAllThreads(CFArrayRef threads)
{
    for (CFIndex idx = 0; idx < CFArrayGetCount(threads); idx++)
    {
        thread_act_t thread = (thread_act_t)(uintptr_t)CFArrayGetValueAtIndex(threads, idx);
        kern_return_t ret = thread_resume(thread);
        if (ret != KERN_SUCCESS)
            CRASH("*** Failure from thread_resume (%d) ***", ret);
        mach_port_deallocate(mach_task_self(), thread);
    }
}

static uint32_t __CF_last_warned_port_count = 0;

static void foo() __attribute__((unused));
static void foo()
{
    uint32_t pcnt = __CFGetProcessPortCount();
    if (__CF_last_warned_port_count + 1000 < pcnt)
    {
        CFArrayRef threads = __CFStopAllThreads();

        // do stuff here
        CFOptionFlags responseFlags = 0;
        SInt32 result = CFUserNotificationDisplayAlert(0.0, kCFUserNotificationCautionAlertLevel, NULL, NULL, NULL, CFSTR("High Mach Port Usage"), CFSTR("This application is using a lot of Mach ports."), CFSTR("Default"), CFSTR("Altern"), CFSTR("Other b"), &responseFlags);
        if (0 != result)
        {
            CFLog(3, CFSTR("ERROR"));
        }
        else
        {
            switch (responseFlags)
            {
            case kCFUserNotificationDefaultResponse:
                CFLog(3, CFSTR("DefaultR"));
                break;
            case kCFUserNotificationAlternateResponse:
                CFLog(3, CFSTR("AltR"));
                break;
            case kCFUserNotificationOtherResponse:
                CFLog(3, CFSTR("OtherR"));
                break;
            case kCFUserNotificationCancelResponse:
                CFLog(3, CFSTR("CancelR"));
                break;
            }
        }

        __CFRestartAllThreads(threads);
        CFRelease(threads);
        __CF_last_warned_port_count = pcnt;
    }
}

typedef mach_port_t __CFPort;
#define CFPORT_NULL MACH_PORT_NULL
typedef mach_port_t __CFPortSet;

static void __THE_SYSTEM_HAS_NO_PORTS_AVAILABLE__(kern_return_t ret) __attribute__((noinline));
static void __THE_SYSTEM_HAS_NO_PORTS_AVAILABLE__(kern_return_t ret) { HALT; };

static __CFPort __CFPortAllocate(void)
{
    __CFPort result = CFPORT_NULL;
    kern_return_t ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &result);
    if (KERN_SUCCESS != ret)
    {
        char msg[256];
        snprintf(msg, 256, "*** The system has no mach ports available. You may be able to diagnose which application(s) are using ports by using 'top' or Activity Monitor. (%d) ***", ret);
        CRSetCrashLogMessage(msg);
        __THE_SYSTEM_HAS_NO_PORTS_AVAILABLE__(ret);
        return CFPORT_NULL;
    }

    ret = mach_port_insert_right(mach_task_self(), result, result, MACH_MSG_TYPE_MAKE_SEND);
    if (KERN_SUCCESS != ret)
        CRASH("*** Unable to set send right on mach port. (%d) ***", ret);

    mach_port_limits_t limits;
    limits.mpl_qlimit = 1;
    ret = mach_port_set_attributes(mach_task_self(), result, MACH_PORT_LIMITS_INFO, (mach_port_info_t)&limits, MACH_PORT_LIMITS_INFO_COUNT);
    if (KERN_SUCCESS != ret)
        CRASH("*** Unable to set attributes on mach port. (%d) ***", ret);

    return result;
}

CF_INLINE void __CFPortFree(__CFPort port)
{
    mach_port_destroy(mach_task_self(), port);
}

static void __THE_SYSTEM_HAS_NO_PORT_SETS_AVAILABLE__(kern_return_t ret) __attribute__((noinline));
static void __THE_SYSTEM_HAS_NO_PORT_SETS_AVAILABLE__(kern_return_t ret) { HALT; };

CF_INLINE __CFPortSet __CFPortSetAllocate(void)
{
    __CFPortSet result;
    kern_return_t ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &result);
    if (KERN_SUCCESS != ret)
    {
        __THE_SYSTEM_HAS_NO_PORT_SETS_AVAILABLE__(ret);
    }
    return (KERN_SUCCESS == ret) ? result : CFPORT_NULL;
}

CF_INLINE kern_return_t __CFPortSetInsert(__CFPort port, __CFPortSet portSet)
{
    if (MACH_PORT_NULL == port)
    {
        return -1;
    }
    return mach_port_insert_member(mach_task_self(), port, portSet);
}

CF_INLINE kern_return_t __CFPortSetRemove(__CFPort port, __CFPortSet portSet)
{
    if (MACH_PORT_NULL == port)
    {
        return -1;
    }
    return mach_port_extract_member(mach_task_self(), port, portSet);
}

CF_INLINE void __CFPortSetFree(__CFPortSet portSet)
{
    kern_return_t ret;
    mach_port_name_array_t array;
    mach_msg_type_number_t idx, number;

    ret = mach_port_get_set_status(mach_task_self(), portSet, &array, &number);
    if (KERN_SUCCESS == ret)
    {
        for (idx = 0; idx < number; idx++)
        {
            mach_port_extract_member(mach_task_self(), array[idx], portSet);
        }
        vm_deallocate(mach_task_self(), (vm_address_t)array, number * sizeof(mach_port_name_t));
    }
    mach_port_destroy(mach_task_self(), portSet);
}

#elif DEPLOYMENT_TARGET_WINDOWS

typedef HANDLE __CFPort;
#define CFPORT_NULL NULL

// A simple dynamic array of HANDLEs, which grows to a high-water mark
typedef struct ___CFPortSet
{
    uint16_t used;
    uint16_t size;
    HANDLE *handles;
    CFLock_t lock; // insert and remove must be thread safe, like the Mach calls
} * __CFPortSet;

CF_INLINE __CFPort __CFPortAllocate(void)
{
    return CreateEventA(NULL, true, false, NULL);
}

CF_INLINE void __CFPortFree(__CFPort port)
{
    CloseHandle(port);
}

static __CFPortSet __CFPortSetAllocate(void)
{
    __CFPortSet result = (__CFPortSet)CFAllocatorAllocate(kCFAllocatorSystemDefault, sizeof(struct ___CFPortSet), 0);
    result->used = 0;
    result->size = 4;
    result->handles = (HANDLE *)CFAllocatorAllocate(kCFAllocatorSystemDefault, result->size * sizeof(HANDLE), 0);
    CF_SPINLOCK_INIT_FOR_STRUCTS(result->lock);
    return result;
}

static void __CFPortSetFree(__CFPortSet portSet)
{
    CFAllocatorDeallocate(kCFAllocatorSystemDefault, portSet->handles);
    CFAllocatorDeallocate(kCFAllocatorSystemDefault, portSet);
}

// Returns portBuf if ports fit in that space, else returns another ptr that must be freed
static __CFPort *__CFPortSetGetPorts(__CFPortSet portSet, __CFPort *portBuf, uint32_t bufSize, uint32_t *portsUsed)
{
    __CFLock(&(portSet->lock));
    __CFPort *result = portBuf;
    if (bufSize < portSet->used)
        result = (__CFPort *)CFAllocatorAllocate(kCFAllocatorSystemDefault, portSet->used * sizeof(HANDLE), 0);
    if (portSet->used > 1)
    {
        // rotate the ports to vaguely simulate round-robin behaviour
        uint16_t lastPort = portSet->used - 1;
        HANDLE swapHandle = portSet->handles[0];
        memmove(portSet->handles, &portSet->handles[1], lastPort * sizeof(HANDLE));
        portSet->handles[lastPort] = swapHandle;
    }
    memmove(result, portSet->handles, portSet->used * sizeof(HANDLE));
    *portsUsed = portSet->used;
    __CFUnlock(&(portSet->lock));
    return result;
}

static kern_return_t __CFPortSetInsert(__CFPort port, __CFPortSet portSet)
{
    if (NULL == port)
    {
        return -1;
    }
    __CFLock(&(portSet->lock));
    if (portSet->used >= portSet->size)
    {
        portSet->size += 4;
        portSet->handles = (HANDLE *)CFAllocatorReallocate(kCFAllocatorSystemDefault, portSet->handles, portSet->size * sizeof(HANDLE), 0);
    }
    if (portSet->used >= MAXIMUM_WAIT_OBJECTS)
    {
        CFLog(kCFLogLevelWarning, CFSTR("*** More than MAXIMUM_WAIT_OBJECTS (%d) ports add to a port set.  The last ones will be ignored."), MAXIMUM_WAIT_OBJECTS);
    }
    portSet->handles[portSet->used++] = port;
    __CFUnlock(&(portSet->lock));
    return KERN_SUCCESS;
}

static kern_return_t __CFPortSetRemove(__CFPort port, __CFPortSet portSet)
{
    int i, j;
    if (NULL == port)
    {
        return -1;
    }
    __CFLock(&(portSet->lock));
    for (i = 0; i < portSet->used; i++)
    {
        if (portSet->handles[i] == port)
        {
            for (j = i + 1; j < portSet->used; j++)
            {
                portSet->handles[j - 1] = portSet->handles[j];
            }
            portSet->used--;
            __CFUnlock(&(portSet->lock));
            return true;
        }
    }
    __CFUnlock(&(portSet->lock));
    return KERN_SUCCESS;
}

#endif

#if !defined(__MACTYPES__) && !defined(_OS_OSTYPES_H)
#if defined(__BIG_ENDIAN__)
typedef struct UnsignedWide
{
    UInt32 hi;
    UInt32 lo;
} UnsignedWide;
#elif defined(__LITTLE_ENDIAN__)
typedef struct UnsignedWide
{
    UInt32 lo;
    UInt32 hi;
} UnsignedWide;
#endif
typedef UnsignedWide AbsoluteTime;
#endif

#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI

#if USE_DISPATCH_SOURCE_FOR_TIMERS
#endif
#if USE_MK_TIMER_TOO
extern mach_port_name_t mk_timer_create(void);
extern kern_return_t mk_timer_destroy(mach_port_name_t name);
extern kern_return_t mk_timer_arm(mach_port_name_t name, AbsoluteTime expire_time);
extern kern_return_t mk_timer_cancel(mach_port_name_t name, AbsoluteTime *result_time);

CF_INLINE AbsoluteTime __CFUInt64ToAbsoluteTime(uint64_t x)
{
    AbsoluteTime a;
    a.hi = x >> 32;
    a.lo = x & (uint64_t)0xFFFFFFFF;
    return a;
}
#endif

static uint32_t __CFSendTrivialMachMessage(mach_port_t port, uint32_t msg_id, CFOptionFlags options, uint32_t timeout)
{
    kern_return_t result;
    mach_msg_header_t header;
    header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    header.msgh_size = sizeof(mach_msg_header_t);
    header.msgh_remote_port = port;
    header.msgh_local_port = MACH_PORT_NULL;
    header.msgh_id = msg_id;
    result = mach_msg(&header, MACH_SEND_MSG | options, header.msgh_size, 0, MACH_PORT_NULL, timeout, MACH_PORT_NULL);
    if (result == MACH_SEND_TIMED_OUT)
        mach_msg_destroy(&header);
    return result;
}
#elif DEPLOYMENT_TARGET_WINDOWS

static HANDLE mk_timer_create(void)
{
    return CreateWaitableTimer(NULL, FALSE, NULL);
}

static kern_return_t mk_timer_destroy(HANDLE name)
{
    BOOL res = CloseHandle(name);
    if (!res)
    {
        DWORD err = GetLastError();
        CFLog(kCFLogLevelError, CFSTR("CFRunLoop: Unable to destroy timer: %d"), err);
    }
    return (int)res;
}

static kern_return_t mk_timer_arm(HANDLE name, LARGE_INTEGER expire_time)
{
    BOOL res = SetWaitableTimer(name, &expire_time, 0, NULL, NULL, FALSE);
    if (!res)
    {
        DWORD err = GetLastError();
        CFLog(kCFLogLevelError, CFSTR("CFRunLoop: Unable to set timer: %d"), err);
    }
    return (int)res;
}

static kern_return_t mk_timer_cancel(HANDLE name, LARGE_INTEGER *result_time)
{
    BOOL res = CancelWaitableTimer(name);
    if (!res)
    {
        DWORD err = GetLastError();
        CFLog(kCFLogLevelError, CFSTR("CFRunLoop: Unable to cancel timer: %d"), err);
    }
    return (int)res;
}

// The name of this function is a lie on Windows. The return value matches the argument of the fake mk_timer functions above. Note that the Windows timers expect to be given "system time". We have to do some calculations to get the right value, which is a FILETIME-like value.
CF_INLINE LARGE_INTEGER __CFUInt64ToAbsoluteTime(uint64_t desiredFireTime)
{
    LARGE_INTEGER result;
    // There is a race we know about here, (timer fire time calculated -> thread suspended -> timer armed == late timer fire), but we don't have a way to avoid it at this time, since the only way to specify an absolute value to the timer is to calculate the relative time first. Fixing that would probably require not using the TSR for timers on Windows.
    uint64_t now = mach_absolute_time();
    if (now > desiredFireTime)
    {
        result.QuadPart = 0;
    }
    else
    {
        uint64_t timeDiff = desiredFireTime - now;
        CFTimeInterval amountOfTimeToWait = __CFTSRToTimeInterval(timeDiff);
        // Result is in 100 ns (10**-7 sec) units to be consistent with a FILETIME.
        // CFTimeInterval is in seconds.
        result.QuadPart = -(amountOfTimeToWait * 10000000);
    }
    return result;
}

#endif

#pragma mark -
#pragma mark Modes

/* unlock a run loop and modes before doing callouts/sleeping */
/* never try to take the run loop lock with a mode locked */
/* be very careful of common subexpression elimination and compacting code, particular across locks and unlocks! */
/* run loop mode structures should never be deallocated, even if they become empty */

static CFTypeID __kCFRunLoopModeTypeID = _kCFRuntimeNotATypeID;
static CFTypeID __kCFRunLoopTypeID = _kCFRuntimeNotATypeID;
static CFTypeID __kCFRunLoopSourceTypeID = _kCFRuntimeNotATypeID;
static CFTypeID __kCFRunLoopObserverTypeID = _kCFRuntimeNotATypeID;
static CFTypeID __kCFRunLoopTimerTypeID = _kCFRuntimeNotATypeID;

typedef struct __CFRunLoopMode *CFRunLoopModeRef;

struct __CFRunLoopMode
{
    CFRuntimeBase _base;
    pthread_mutex_t _lock; /* must have the run loop locked before locking this */
    CFStringRef _name;
    Boolean _stopped;
    char _padding[3];
    CFMutableSetRef _sources0;
    CFMutableSetRef _sources1;
    CFMutableArrayRef _observers;
    CFMutableArrayRef _timers;
    CFMutableDictionaryRef _portToV1SourceMap;
    __CFPortSet _portSet;
    CFIndex _observerMask;
#if USE_DISPATCH_SOURCE_FOR_TIMERS
    dispatch_source_t _timerSource;
    dispatch_queue_t _queue;
    Boolean _timerFired; // set to true by the source when a timer has fired
    Boolean _dispatchTimerArmed;
#endif
#if USE_MK_TIMER_TOO
    mach_port_t _timerPort;
    Boolean _mkTimerArmed;
#endif
#if DEPLOYMENT_TARGET_WINDOWS
    DWORD _msgQMask;
    void (*_msgPump)(void);
#endif
    uint64_t _timerSoftDeadline; /* TSR */
    uint64_t _timerHardDeadline; /* TSR */
};

CF_INLINE void __CFRunLoopModeLock(CFRunLoopModeRef rlm)
{
    pthread_mutex_lock(&(rlm->_lock));
    // CFLog(6, CFSTR("__CFRunLoopModeLock locked %p"), rlm);
}

CF_INLINE void __CFRunLoopModeUnlock(CFRunLoopModeRef rlm)
{
    // CFLog(6, CFSTR("__CFRunLoopModeLock unlocking %p"), rlm);
    pthread_mutex_unlock(&(rlm->_lock));
}

static Boolean __CFRunLoopModeEqual(CFTypeRef cf1, CFTypeRef cf2)
{
    CFRunLoopModeRef rlm1 = (CFRunLoopModeRef)cf1;
    CFRunLoopModeRef rlm2 = (CFRunLoopModeRef)cf2;
    return CFEqual(rlm1->_name, rlm2->_name);
}

static CFHashCode __CFRunLoopModeHash(CFTypeRef cf)
{
    CFRunLoopModeRef rlm = (CFRunLoopModeRef)cf;
    return CFHash(rlm->_name);
}

static CFStringRef __CFRunLoopModeCopyDescription(CFTypeRef cf)
{
    CFRunLoopModeRef rlm = (CFRunLoopModeRef)cf;
    CFMutableStringRef result;
    result = CFStringCreateMutable(kCFAllocatorSystemDefault, 0);
    CFStringAppendFormat(result, NULL, CFSTR("<CFRunLoopMode %p [%p]>{name = %@, "), rlm, CFGetAllocator(rlm), rlm->_name);
    CFStringAppendFormat(result, NULL, CFSTR("port set = 0x%x, "), rlm->_portSet);
#if USE_DISPATCH_SOURCE_FOR_TIMERS
    CFStringAppendFormat(result, NULL, CFSTR("queue = %p, "), rlm->_queue);
    CFStringAppendFormat(result, NULL, CFSTR("source = %p (%s), "), rlm->_timerSource, rlm->_timerFired ? "fired" : "not fired");
#endif
#if USE_MK_TIMER_TOO
    CFStringAppendFormat(result, NULL, CFSTR("timer port = 0x%x, "), rlm->_timerPort);
#endif
#if DEPLOYMENT_TARGET_WINDOWS
    CFStringAppendFormat(result, NULL, CFSTR("MSGQ mask = %p, "), rlm->_msgQMask);
#endif
    CFStringAppendFormat(result, NULL, CFSTR("\n\tsources0 = %@,\n\tsources1 = %@,\n\tobservers = %@,\n\ttimers = %@,\n\tcurrently %0.09g (%lld) / soft deadline in: %0.09g sec (@ %lld) / hard deadline in: %0.09g sec (@ %lld)\n},\n"), rlm->_sources0, rlm->_sources1, rlm->_observers, rlm->_timers, CFAbsoluteTimeGetCurrent(), mach_absolute_time(), __CFTSRToTimeInterval(rlm->_timerSoftDeadline - mach_absolute_time()), rlm->_timerSoftDeadline, __CFTSRToTimeInterval(rlm->_timerHardDeadline - mach_absolute_time()), rlm->_timerHardDeadline);
    return result;
}

static void __CFRunLoopModeDeallocate(CFTypeRef cf)
{
    CFRunLoopModeRef rlm = (CFRunLoopModeRef)cf;
    if (NULL != rlm->_sources0)
        CFRelease(rlm->_sources0);
    if (NULL != rlm->_sources1)
        CFRelease(rlm->_sources1);
    if (NULL != rlm->_observers)
        CFRelease(rlm->_observers);
    if (NULL != rlm->_timers)
        CFRelease(rlm->_timers);
    if (NULL != rlm->_portToV1SourceMap)
        CFRelease(rlm->_portToV1SourceMap);
    CFRelease(rlm->_name);
    __CFPortSetFree(rlm->_portSet);
#if USE_DISPATCH_SOURCE_FOR_TIMERS
    if (rlm->_timerSource)
    {
        dispatch_source_cancel(rlm->_timerSource);
        dispatch_release(rlm->_timerSource);
    }
    if (rlm->_queue)
    {
        dispatch_release(rlm->_queue);
    }
#endif
#if USE_MK_TIMER_TOO
    if (MACH_PORT_NULL != rlm->_timerPort)
        mk_timer_destroy(rlm->_timerPort);
#endif
    pthread_mutex_destroy(&rlm->_lock);
    memset((char *)cf + sizeof(CFRuntimeBase), 0x7C, sizeof(struct __CFRunLoopMode) - sizeof(CFRuntimeBase));
}

#pragma mark -
#pragma mark Run Loops

struct _block_item
{
    struct _block_item *_next;
    CFTypeRef _mode; // CFString or CFSet
    void (^_block)(void);
};

typedef struct _per_run_data
{
    uint32_t a;
    uint32_t b;
    uint32_t stopped;
    uint32_t ignoreWakeUps;
} _per_run_data;

struct __CFRunLoop
{
    CFRuntimeBase _base;
    pthread_mutex_t _lock; /* locked for accessing mode list */
    __CFPort _wakeUpPort;  // used for CFRunLoopWakeUp
    Boolean _unused;
    volatile _per_run_data *_perRunData; // reset for runs of the run loop
    pthread_t _pthread;
    uint32_t _winthread;
    CFMutableSetRef _commonModes;
    CFMutableSetRef _commonModeItems;
    CFRunLoopModeRef _currentMode;
    CFMutableSetRef _modes;
    struct _block_item *_blocks_head;
    struct _block_item *_blocks_tail;
    CFAbsoluteTime _runTime;
    CFAbsoluteTime _sleepTime;
    CFTypeRef _counterpart;
};

/* Bit 0 of the base reserved bits is used for stopped state */
/* Bit 1 of the base reserved bits is used for sleeping state */
/* Bit 2 of the base reserved bits is used for deallocating state */

CF_INLINE volatile _per_run_data *__CFRunLoopPushPerRunData(CFRunLoopRef rl)
{
    volatile _per_run_data *previous = rl->_perRunData;
    rl->_perRunData = (volatile _per_run_data *)CFAllocatorAllocate(kCFAllocatorSystemDefault, sizeof(_per_run_data), 0);
    rl->_perRunData->a = 0x4346524C;
    rl->_perRunData->b = 0x4346524C; // 'CFRL'
    rl->_perRunData->stopped = 0x00000000;
    rl->_perRunData->ignoreWakeUps = 0x00000000;
    return previous;
}

CF_INLINE void __CFRunLoopPopPerRunData(CFRunLoopRef rl, volatile _per_run_data *previous)
{
    if (rl->_perRunData)
        CFAllocatorDeallocate(kCFAllocatorSystemDefault, (void *)rl->_perRunData);
    rl->_perRunData = previous;
}

CF_INLINE Boolean __CFRunLoopIsStopped(CFRunLoopRef rl)
{
    return (rl->_perRunData->stopped) ? true : false;
}

CF_INLINE void __CFRunLoopSetStopped(CFRunLoopRef rl)
{
    rl->_perRunData->stopped = 0x53544F50; // 'STOP'
}

CF_INLINE void __CFRunLoopUnsetStopped(CFRunLoopRef rl)
{
    rl->_perRunData->stopped = 0x0;
}

CF_INLINE Boolean __CFRunLoopIsIgnoringWakeUps(CFRunLoopRef rl)
{
    return (rl->_perRunData->ignoreWakeUps) ? true : false;
}

CF_INLINE void __CFRunLoopSetIgnoreWakeUps(CFRunLoopRef rl)
{
    rl->_perRunData->ignoreWakeUps = 0x57414B45; // 'WAKE'
}

CF_INLINE void __CFRunLoopUnsetIgnoreWakeUps(CFRunLoopRef rl)
{
    rl->_perRunData->ignoreWakeUps = 0x0;
}

CF_INLINE Boolean __CFRunLoopIsSleeping(CFRunLoopRef rl)
{
    return (Boolean)__CFBitfieldGetValue(((const CFRuntimeBase *)rl)->_cfinfo[CF_INFO_BITS], 1, 1);
}

CF_INLINE void __CFRunLoopSetSleeping(CFRunLoopRef rl)
{
    __CFBitfieldSetValue(((CFRuntimeBase *)rl)->_cfinfo[CF_INFO_BITS], 1, 1, 1);
}

CF_INLINE void __CFRunLoopUnsetSleeping(CFRunLoopRef rl)
{
    __CFBitfieldSetValue(((CFRuntimeBase *)rl)->_cfinfo[CF_INFO_BITS], 1, 1, 0);
}

CF_INLINE Boolean __CFRunLoopIsDeallocating(CFRunLoopRef rl)
{
    return (Boolean)__CFBitfieldGetValue(((const CFRuntimeBase *)rl)->_cfinfo[CF_INFO_BITS], 2, 2);
}

CF_INLINE void __CFRunLoopSetDeallocating(CFRunLoopRef rl)
{
    __CFBitfieldSetValue(((CFRuntimeBase *)rl)->_cfinfo[CF_INFO_BITS], 2, 2, 1);
}

CF_INLINE void __CFRunLoopLock(CFRunLoopRef rl)
{
    pthread_mutex_lock(&(((CFRunLoopRef)rl)->_lock));
    //    CFLog(6, CFSTR("__CFRunLoopLock locked %p"), rl);
}

CF_INLINE void __CFRunLoopUnlock(CFRunLoopRef rl)
{
    //    CFLog(6, CFSTR("__CFRunLoopLock unlocking %p"), rl);
    pthread_mutex_unlock(&(((CFRunLoopRef)rl)->_lock));
}

static CFStringRef __CFRunLoopCopyDescription(CFTypeRef cf)
{
    CFRunLoopRef rl = (CFRunLoopRef)cf;
    CFMutableStringRef result;
    result = CFStringCreateMutable(kCFAllocatorSystemDefault, 0);
#if DEPLOYMENT_TARGET_WINDOWS
    CFStringAppendFormat(result, NULL, CFSTR("<CFRunLoop %p [%p]>{wakeup port = 0x%x, stopped = %s, ignoreWakeUps = %s, \ncurrent mode = %@,\n"), cf, CFGetAllocator(cf), rl->_wakeUpPort, __CFRunLoopIsStopped(rl) ? "true" : "false", __CFRunLoopIsIgnoringWakeUps(rl) ? "true" : "false", rl->_currentMode ? rl->_currentMode->_name : CFSTR("(none)"));
#else
    CFStringAppendFormat(result, NULL, CFSTR("<CFRunLoop %p [%p]>{wakeup port = 0x%x, stopped = %s, ignoreWakeUps = %s, \ncurrent mode = %@,\n"), cf, CFGetAllocator(cf), rl->_wakeUpPort, __CFRunLoopIsStopped(rl) ? "true" : "false", __CFRunLoopIsIgnoringWakeUps(rl) ? "true" : "false", rl->_currentMode ? rl->_currentMode->_name : CFSTR("(none)"));
#endif
    CFStringAppendFormat(result, NULL, CFSTR("common modes = %@,\ncommon mode items = %@,\nmodes = %@}\n"), rl->_commonModes, rl->_commonModeItems, rl->_modes);
    return result;
}

CF_PRIVATE void __CFRunLoopDump()
{ // __private_extern__ to keep the compiler from discarding it
    CFShow(CFCopyDescription(CFRunLoopGetCurrent()));
}

CF_INLINE void __CFRunLoopLockInit(pthread_mutex_t *lock)
{
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
    int32_t mret = pthread_mutex_init(lock, &mattr);
    pthread_mutexattr_destroy(&mattr);
    if (0 != mret)
    {
    }
}

/* call with rl locked, returns mode locked */
static CFRunLoopModeRef __CFRunLoopFindMode(CFRunLoopRef rl, CFStringRef modeName, Boolean create)
{
    CHECK_FOR_FORK();
    CFRunLoopModeRef rlm;
    struct __CFRunLoopMode srlm;
    memset(&srlm, 0, sizeof(srlm));
    _CFRuntimeSetInstanceTypeIDAndIsa(&srlm, __kCFRunLoopModeTypeID);
    srlm._name = modeName;
    rlm = (CFRunLoopModeRef)CFSetGetValue(rl->_modes, &srlm);
    if (NULL != rlm)
    {
        __CFRunLoopModeLock(rlm);
        return rlm;
    }
    if (!create)
    {
        return NULL;
    }
    rlm = (CFRunLoopModeRef)_CFRuntimeCreateInstance(kCFAllocatorSystemDefault, __kCFRunLoopModeTypeID, sizeof(struct __CFRunLoopMode) - sizeof(CFRuntimeBase), NULL);
    if (NULL == rlm)
    {
        return NULL;
    }
    __CFRunLoopLockInit(&rlm->_lock);
    rlm->_name = CFStringCreateCopy(kCFAllocatorSystemDefault, modeName);
    rlm->_stopped = false;
    rlm->_portToV1SourceMap = NULL;
    rlm->_sources0 = NULL;
    rlm->_sources1 = NULL;
    rlm->_observers = NULL;
    rlm->_timers = NULL;
    rlm->_observerMask = 0;
    rlm->_portSet = __CFPortSetAllocate();
    rlm->_timerSoftDeadline = UINT64_MAX;
    rlm->_timerHardDeadline = UINT64_MAX;

    kern_return_t ret = KERN_SUCCESS;
#if USE_DISPATCH_SOURCE_FOR_TIMERS
    rlm->_timerFired = false;
    rlm->_queue = _dispatch_runloop_root_queue_create_4CF("Run Loop Mode Queue", 0);
    mach_port_t queuePort = _dispatch_runloop_root_queue_get_port_4CF(rlm->_queue);
    if (queuePort == MACH_PORT_NULL)
        CRASH("*** Unable to create run loop mode queue port. (%d) ***", -1);
    rlm->_timerSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, rlm->_queue);

    __block Boolean *timerFiredPointer = &(rlm->_timerFired);
    dispatch_source_set_event_handler(rlm->_timerSource, ^{
      *timerFiredPointer = true;
    });

    // Set timer to far out there. The unique leeway makes this timer easy to spot in debug output.
    _dispatch_source_set_runloop_timer_4CF(rlm->_timerSource, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 321);
    dispatch_resume(rlm->_timerSource);

    ret = __CFPortSetInsert(queuePort, rlm->_portSet);
    if (KERN_SUCCESS != ret)
        CRASH("*** Unable to insert timer port into port set. (%d) ***", ret);

#endif
#if USE_MK_TIMER_TOO
    rlm->_timerPort = mk_timer_create();
    ret = __CFPortSetInsert(rlm->_timerPort, rlm->_portSet);
    if (KERN_SUCCESS != ret)
        CRASH("*** Unable to insert timer port into port set. (%d) ***", ret);
#endif

    ret = __CFPortSetInsert(rl->_wakeUpPort, rlm->_portSet);
    if (KERN_SUCCESS != ret)
        CRASH("*** Unable to insert wake up port into port set. (%d) ***", ret);

#if DEPLOYMENT_TARGET_WINDOWS
    rlm->_msgQMask = 0;
    rlm->_msgPump = NULL;
#endif
    CFSetAddValue(rl->_modes, rlm);
    CFRelease(rlm);
    __CFRunLoopModeLock(rlm); /* return mode locked */
    return rlm;
}

// expects rl and rlm locked
static Boolean __CFRunLoopModeIsEmpty(CFRunLoopRef rl, CFRunLoopModeRef rlm, CFRunLoopModeRef previousMode)
{
    CHECK_FOR_FORK();
    if (NULL == rlm)
        return true;
#if DEPLOYMENT_TARGET_WINDOWS
    if (0 != rlm->_msgQMask)
        return false;
#endif
    Boolean libdispatchQSafe = pthread_main_np() && ((HANDLE_DISPATCH_ON_BASE_INVOCATION_ONLY && NULL == previousMode) || (!HANDLE_DISPATCH_ON_BASE_INVOCATION_ONLY && 0 == _CFGetTSD(__CFTSDKeyIsInGCDMainQ)));
    if (libdispatchQSafe && (CFRunLoopGetMain() == rl) && CFSetContainsValue(rl->_commonModes, rlm->_name))
        return false; // represents the libdispatch main queue
    if (NULL != rlm->_sources0 && 0 < CFSetGetCount(rlm->_sources0))
        return false;
    if (NULL != rlm->_sources1 && 0 < CFSetGetCount(rlm->_sources1))
        return false;
    if (NULL != rlm->_timers && 0 < CFArrayGetCount(rlm->_timers))
        return false;
    struct _block_item *item = rl->_blocks_head;
    while (item)
    {
        struct _block_item *curr = item;
        item = item->_next;
        Boolean doit = false;
        if (CFStringGetTypeID() == CFGetTypeID(curr->_mode))
        {
            doit = CFEqual(curr->_mode, rlm->_name) || (CFEqual(curr->_mode, kCFRunLoopCommonModes) && CFSetContainsValue(rl->_commonModes, rlm->_name));
        }
        else
        {
            doit = CFSetContainsValue((CFSetRef)curr->_mode, rlm->_name) || (CFSetContainsValue((CFSetRef)curr->_mode, kCFRunLoopCommonModes) && CFSetContainsValue(rl->_commonModes, rlm->_name));
        }
        if (doit)
            return false;
    }
    return true;
}

#if DEPLOYMENT_TARGET_WINDOWS

uint32_t _CFRunLoopGetWindowsMessageQueueMask(CFRunLoopRef rl, CFStringRef modeName)
{
    if (modeName == kCFRunLoopCommonModes)
    {
        CFLog(kCFLogLevelError, CFSTR("_CFRunLoopGetWindowsMessageQueueMask: kCFRunLoopCommonModes unsupported"));
        HALT;
    }
    DWORD result = 0;
    __CFRunLoopLock(rl);
    CFRunLoopModeRef rlm = __CFRunLoopFindMode(rl, modeName, false);
    if (rlm)
    {
        result = rlm->_msgQMask;
        __CFRunLoopModeUnlock(rlm);
    }
    __CFRunLoopUnlock(rl);
    return (uint32_t)result;
}

void _CFRunLoopSetWindowsMessageQueueMask(CFRunLoopRef rl, uint32_t mask, CFStringRef modeName)
{
    if (modeName == kCFRunLoopCommonModes)
    {
        CFLog(kCFLogLevelError, CFSTR("_CFRunLoopSetWindowsMessageQueueMask: kCFRunLoopCommonModes unsupported"));
        HALT;
    }
    __CFRunLoopLock(rl);
    CFRunLoopModeRef rlm = __CFRunLoopFindMode(rl, modeName, true);
    rlm->_msgQMask = (DWORD)mask;
    __CFRunLoopModeUnlock(rlm);
    __CFRunLoopUnlock(rl);
}

uint32_t _CFRunLoopGetWindowsThreadID(CFRunLoopRef rl)
{
    return rl->_winthread;
}

CFWindowsMessageQueueHandler _CFRunLoopGetWindowsMessageQueueHandler(CFRunLoopRef rl, CFStringRef modeName)
{
    if (modeName == kCFRunLoopCommonModes)
    {
        CFLog(kCFLogLevelError, CFSTR("_CFRunLoopGetWindowsMessageQueueMask: kCFRunLoopCommonModes unsupported"));
        HALT;
    }
    if (rl != CFRunLoopGetCurrent())
    {
        CFLog(kCFLogLevelError, CFSTR("_CFRunLoopGetWindowsMessageQueueHandler: run loop parameter must be the current run loop"));
        HALT;
    }
    void (*result)(void) = NULL;
    __CFRunLoopLock(rl);
    CFRunLoopModeRef rlm = __CFRunLoopFindMode(rl, modeName, false);
    if (rlm)
    {
        result = rlm->_msgPump;
        __CFRunLoopModeUnlock(rlm);
    }
    __CFRunLoopUnlock(rl);
    return result;
}

void _CFRunLoopSetWindowsMessageQueueHandler(CFRunLoopRef rl, CFStringRef modeName, CFWindowsMessageQueueHandler func)
{
    if (modeName == kCFRunLoopCommonModes)
    {
        CFLog(kCFLogLevelError, CFSTR("_CFRunLoopGetWindowsMessageQueueMask: kCFRunLoopCommonModes unsupported"));
        HALT;
    }
    if (rl != CFRunLoopGetCurrent())
    {
        CFLog(kCFLogLevelError, CFSTR("_CFRunLoopGetWindowsMessageQueueHandler: run loop parameter must be the current run loop"));
        HALT;
    }
    __CFRunLoopLock(rl);
    CFRunLoopModeRef rlm = __CFRunLoopFindMode(rl, modeName, true);
    rlm->_msgPump = func;
    __CFRunLoopModeUnlock(rlm);
    __CFRunLoopUnlock(rl);
}

#endif

#pragma mark -
#pragma mark Sources

/* Bit 3 in the base reserved bits is used for invalid state in run loop objects */

CF_INLINE Boolean __CFIsValid(const void *cf)
{
    return (Boolean)__CFBitfieldGetValue(((const CFRuntimeBase *)cf)->_cfinfo[CF_INFO_BITS], 3, 3);
}

CF_INLINE void __CFSetValid(void *cf)
{
    __CFBitfieldSetValue(((CFRuntimeBase *)cf)->_cfinfo[CF_INFO_BITS], 3, 3, 1);
}

CF_INLINE void __CFUnsetValid(void *cf)
{
    __CFBitfieldSetValue(((CFRuntimeBase *)cf)->_cfinfo[CF_INFO_BITS], 3, 3, 0);
}

struct __CFRunLoopSource
{
    CFRuntimeBase _base;
    uint32_t _bits;
    pthread_mutex_t _lock;
    CFIndex _order; /* immutable */
    CFMutableBagRef _runLoops;
    union
    {
        CFRunLoopSourceContext version0;  /* immutable, except invalidation */
        CFRunLoopSourceContext1 version1; /* immutable, except invalidation */
    } _context;
};

/* Bit 1 of the base reserved bits is used for signalled state */

CF_INLINE Boolean __CFRunLoopSourceIsSignaled(CFRunLoopSourceRef rls)
{
    return (Boolean)__CFBitfieldGetValue(rls->_bits, 1, 1);
}

CF_INLINE void __CFRunLoopSourceSetSignaled(CFRunLoopSourceRef rls)
{
    __CFBitfieldSetValue(rls->_bits, 1, 1, 1);
}

CF_INLINE void __CFRunLoopSourceUnsetSignaled(CFRunLoopSourceRef rls)
{
    __CFBitfieldSetValue(rls->_bits, 1, 1, 0);
}

CF_INLINE void __CFRunLoopSourceLock(CFRunLoopSourceRef rls)
{
    pthread_mutex_lock(&(rls->_lock));
    //    CFLog(6, CFSTR("__CFRunLoopSourceLock locked %p"), rls);
}

CF_INLINE void __CFRunLoopSourceUnlock(CFRunLoopSourceRef rls)
{
    //    CFLog(6, CFSTR("__CFRunLoopSourceLock unlocking %p"), rls);
    pthread_mutex_unlock(&(rls->_lock));
}

#pragma mark Observers

struct __CFRunLoopObserver
{
    CFRuntimeBase _base;
    pthread_mutex_t _lock;
    CFRunLoopRef _runLoop;
    CFIndex _rlCount;
    CFOptionFlags _activities;          /* immutable */
    CFIndex _order;                     /* immutable */
    CFRunLoopObserverCallBack _callout; /* immutable */
    CFRunLoopObserverContext _context;  /* immutable, except invalidation */
};

/* Bit 0 of the base reserved bits is used for firing state */
/* Bit 1 of the base reserved bits is used for repeats state */

CF_INLINE Boolean __CFRunLoopObserverIsFiring(CFRunLoopObserverRef rlo)
{
    return (Boolean)__CFBitfieldGetValue(((const CFRuntimeBase *)rlo)->_cfinfo[CF_INFO_BITS], 0, 0);
}

CF_INLINE void __CFRunLoopObserverSetFiring(CFRunLoopObserverRef rlo)
{
    __CFBitfieldSetValue(((CFRuntimeBase *)rlo)->_cfinfo[CF_INFO_BITS], 0, 0, 1);
}

CF_INLINE void __CFRunLoopObserverUnsetFiring(CFRunLoopObserverRef rlo)
{
    __CFBitfieldSetValue(((CFRuntimeBase *)rlo)->_cfinfo[CF_INFO_BITS], 0, 0, 0);
}

CF_INLINE Boolean __CFRunLoopObserverRepeats(CFRunLoopObserverRef rlo)
{
    return (Boolean)__CFBitfieldGetValue(((const CFRuntimeBase *)rlo)->_cfinfo[CF_INFO_BITS], 1, 1);
}

CF_INLINE void __CFRunLoopObserverSetRepeats(CFRunLoopObserverRef rlo)
{
    __CFBitfieldSetValue(((CFRuntimeBase *)rlo)->_cfinfo[CF_INFO_BITS], 1, 1, 1);
}

CF_INLINE void __CFRunLoopObserverUnsetRepeats(CFRunLoopObserverRef rlo)
{
    __CFBitfieldSetValue(((CFRuntimeBase *)rlo)->_cfinfo[CF_INFO_BITS], 1, 1, 0);
}

CF_INLINE void __CFRunLoopObserverLock(CFRunLoopObserverRef rlo)
{
    pthread_mutex_lock(&(rlo->_lock));
    //    CFLog(6, CFSTR("__CFRunLoopObserverLock locked %p"), rlo);
}

CF_INLINE void __CFRunLoopObserverUnlock(CFRunLoopObserverRef rlo)
{
    //    CFLog(6, CFSTR("__CFRunLoopObserverLock unlocking %p"), rlo);
    pthread_mutex_unlock(&(rlo->_lock));
}

static void __CFRunLoopObserverSchedule(CFRunLoopObserverRef rlo, CFRunLoopRef rl, CFRunLoopModeRef rlm)
{
    __CFRunLoopObserverLock(rlo);
    if (0 == rlo->_rlCount)
    {
        rlo->_runLoop = rl;
    }
    rlo->_rlCount++;
    __CFRunLoopObserverUnlock(rlo);
}

static void __CFRunLoopObserverCancel(CFRunLoopObserverRef rlo, CFRunLoopRef rl, CFRunLoopModeRef rlm)
{
    __CFRunLoopObserverLock(rlo);
    rlo->_rlCount--;
    if (0 == rlo->_rlCount)
    {
        rlo->_runLoop = NULL;
    }
    __CFRunLoopObserverUnlock(rlo);
}

#pragma mark Timers

/*
NSTimer 是与 run loop 息息相关的，CFRunLoopTimerRef 与 NSTimer 是可以 toll-free bridged（免费桥接转换）的。
当 timer 加到 run loop 的时候，run loop 会注册对应的触发时间点，
时间到了，run loop 若处于休眠则会被唤醒，执行 timer 对应的回调函数。
*/
struct __CFRunLoopTimer
{
    CFRuntimeBase _base;             // 所有 CF "instances" 都是从这个结构开始的
    uint16_t _bits;                  // 标记 timer 的状态
    pthread_mutex_t _lock;           // 互斥锁
    CFRunLoopRef _runLoop;           // timer 对应的 run loop，注册在哪个 run loop 中
    CFMutableSetRef _rlModes;        // timer 对应的 run loop modes，内部保存的也是 run loop mode 的名字，也验证了 timer 可以在多个 run loop mode 中使用
    CFAbsoluteTime _nextFireDate;    // timer 的下次触发时机，每次触发后都会再次设置该值
    CFTimeInterval _interval;        // timer 的时间间隔 /* immutable */// timer 的时间间隔
    CFTimeInterval _tolerance;       // timer 的允许时间偏差 /* mutable */
    uint64_t _fireTSR;               // timer 本次被触发的时间点 /* TSR units */
    CFIndex _order;                  // timer 优先级 /* immutable */
    CFRunLoopTimerCallBack _callout; // timer 回调 /* immutable */
    CFRunLoopTimerContext _context;  // timer 上下文，可用于传递参数到 timer 对象的回调函数中。 /* immutable, except invalidation */
};

/* Bit 0 of the base reserved bits is used for firing state */
/* Bit 1 of the base reserved bits is used for fired-during-callout state */
/* Bit 2 of the base reserved bits is used for waking state */

CF_INLINE Boolean __CFRunLoopTimerIsFiring(CFRunLoopTimerRef rlt)
{
    return (Boolean)__CFBitfieldGetValue(rlt->_bits, 0, 0);
}

CF_INLINE void __CFRunLoopTimerSetFiring(CFRunLoopTimerRef rlt)
{
    __CFBitfieldSetValue(rlt->_bits, 0, 0, 1);
}

CF_INLINE void __CFRunLoopTimerUnsetFiring(CFRunLoopTimerRef rlt)
{
    __CFBitfieldSetValue(rlt->_bits, 0, 0, 0);
}

CF_INLINE Boolean __CFRunLoopTimerIsDeallocating(CFRunLoopTimerRef rlt)
{
    return (Boolean)__CFBitfieldGetValue(rlt->_bits, 2, 2);
}

CF_INLINE void __CFRunLoopTimerSetDeallocating(CFRunLoopTimerRef rlt)
{
    __CFBitfieldSetValue(rlt->_bits, 2, 2, 1);
}

CF_INLINE void __CFRunLoopTimerLock(CFRunLoopTimerRef rlt)
{
    pthread_mutex_lock(&(rlt->_lock));
    //    CFLog(6, CFSTR("__CFRunLoopTimerLock locked %p"), rlt);
}

CF_INLINE void __CFRunLoopTimerUnlock(CFRunLoopTimerRef rlt)
{
    //    CFLog(6, CFSTR("__CFRunLoopTimerLock unlocking %p"), rlt);
    pthread_mutex_unlock(&(rlt->_lock));
}

static CFLock_t __CFRLTFireTSRLock = CFLockInit;

CF_INLINE void __CFRunLoopTimerFireTSRLock(void)
{
    __CFLock(&__CFRLTFireTSRLock);
}

CF_INLINE void __CFRunLoopTimerFireTSRUnlock(void)
{
    __CFUnlock(&__CFRLTFireTSRLock);
}

#pragma mark -

/* CFRunLoop */

CONST_STRING_DECL(kCFRunLoopDefaultMode, "kCFRunLoopDefaultMode")
CONST_STRING_DECL(kCFRunLoopCommonModes, "kCFRunLoopCommonModes")

// call with rl and rlm locked
static CFRunLoopSourceRef __CFRunLoopModeFindSourceForMachPort(CFRunLoopRef rl, CFRunLoopModeRef rlm, __CFPort port)
{ /* DOES CALLOUT */
    CHECK_FOR_FORK();
    CFRunLoopSourceRef found = rlm->_portToV1SourceMap ? (CFRunLoopSourceRef)CFDictionaryGetValue(rlm->_portToV1SourceMap, (const void *)(uintptr_t)port) : NULL;
    return found;
}

// Remove backreferences the mode's sources have to the rl (context);
// the primary purpose of rls->_runLoops is so that Invalidation can remove
// the source from the run loops it is in, but during deallocation of a
// run loop, we already know that the sources are going to be punted
// from it, so invalidation of sources does not need to remove from a
// deallocating run loop.
static void __CFRunLoopCleanseSources(const void *value, void *context)
{
    CFRunLoopModeRef rlm = (CFRunLoopModeRef)value;
    CFRunLoopRef rl = (CFRunLoopRef)context;
    CFIndex idx, cnt;
    const void **list, *buffer[256];
    if (NULL == rlm->_sources0 && NULL == rlm->_sources1)
        return;

    cnt = (rlm->_sources0 ? CFSetGetCount(rlm->_sources0) : 0) + (rlm->_sources1 ? CFSetGetCount(rlm->_sources1) : 0);
    list = (const void **)((cnt <= 256) ? buffer : CFAllocatorAllocate(kCFAllocatorSystemDefault, cnt * sizeof(void *), 0));
    if (rlm->_sources0)
        CFSetGetValues(rlm->_sources0, list);
    if (rlm->_sources1)
        CFSetGetValues(rlm->_sources1, list + (rlm->_sources0 ? CFSetGetCount(rlm->_sources0) : 0));
    for (idx = 0; idx < cnt; idx++)
    {
        CFRunLoopSourceRef rls = (CFRunLoopSourceRef)list[idx];
        __CFRunLoopSourceLock(rls);
        if (NULL != rls->_runLoops)
        {
            CFBagRemoveValue(rls->_runLoops, rl);
        }
        __CFRunLoopSourceUnlock(rls);
    }
    if (list != buffer)
        CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
}

static void __CFRunLoopDeallocateSources(const void *value, void *context)
{
    CFRunLoopModeRef rlm = (CFRunLoopModeRef)value;
    CFRunLoopRef rl = (CFRunLoopRef)context;
    CFIndex idx, cnt;
    const void **list, *buffer[256];
    if (NULL == rlm->_sources0 && NULL == rlm->_sources1)
        return;

    cnt = (rlm->_sources0 ? CFSetGetCount(rlm->_sources0) : 0) + (rlm->_sources1 ? CFSetGetCount(rlm->_sources1) : 0);
    list = (const void **)((cnt <= 256) ? buffer : CFAllocatorAllocate(kCFAllocatorSystemDefault, cnt * sizeof(void *), 0));
    if (rlm->_sources0)
        CFSetGetValues(rlm->_sources0, list);
    if (rlm->_sources1)
        CFSetGetValues(rlm->_sources1, list + (rlm->_sources0 ? CFSetGetCount(rlm->_sources0) : 0));
    for (idx = 0; idx < cnt; idx++)
    {
        CFRetain(list[idx]);
    }
    if (rlm->_sources0)
        CFSetRemoveAllValues(rlm->_sources0);
    if (rlm->_sources1)
        CFSetRemoveAllValues(rlm->_sources1);
    for (idx = 0; idx < cnt; idx++)
    {
        CFRunLoopSourceRef rls = (CFRunLoopSourceRef)list[idx];
        __CFRunLoopSourceLock(rls);
        if (NULL != rls->_runLoops)
        {
            CFBagRemoveValue(rls->_runLoops, rl);
        }
        __CFRunLoopSourceUnlock(rls);
        if (0 == rls->_context.version0.version)
        {
            if (NULL != rls->_context.version0.cancel)
            {
                rls->_context.version0.cancel(rls->_context.version0.info, rl, rlm->_name); /* CALLOUT */
            }
        }
        else if (1 == rls->_context.version0.version)
        {
            __CFPort port = rls->_context.version1.getPort(rls->_context.version1.info); /* CALLOUT */
            if (CFPORT_NULL != port)
            {
                __CFPortSetRemove(port, rlm->_portSet);
            }
        }
        CFRelease(rls);
    }
    if (list != buffer)
        CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
}

static void __CFRunLoopDeallocateObservers(const void *value, void *context)
{
    CFRunLoopModeRef rlm = (CFRunLoopModeRef)value;
    CFRunLoopRef rl = (CFRunLoopRef)context;
    CFIndex idx, cnt;
    const void **list, *buffer[256];
    if (NULL == rlm->_observers)
        return;
    cnt = CFArrayGetCount(rlm->_observers);
    list = (const void **)((cnt <= 256) ? buffer : CFAllocatorAllocate(kCFAllocatorSystemDefault, cnt * sizeof(void *), 0));
    CFArrayGetValues(rlm->_observers, CFRangeMake(0, cnt), list);
    for (idx = 0; idx < cnt; idx++)
    {
        CFRetain(list[idx]);
    }
    CFArrayRemoveAllValues(rlm->_observers);
    for (idx = 0; idx < cnt; idx++)
    {
        __CFRunLoopObserverCancel((CFRunLoopObserverRef)list[idx], rl, rlm);
        CFRelease(list[idx]);
    }
    if (list != buffer)
        CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
}

static void __CFRunLoopDeallocateTimers(const void *value, void *context)
{
    CFRunLoopModeRef rlm = (CFRunLoopModeRef)value;
    if (NULL == rlm->_timers)
        return;
    void (^deallocateTimers)(CFMutableArrayRef timers) = ^(CFMutableArrayRef timers) {
      CFIndex idx, cnt;
      const void **list, *buffer[256];
      cnt = CFArrayGetCount(timers);
      list = (const void **)((cnt <= 256) ? buffer : CFAllocatorAllocate(kCFAllocatorSystemDefault, cnt * sizeof(void *), 0));
      CFArrayGetValues(timers, CFRangeMake(0, CFArrayGetCount(timers)), list);
      for (idx = 0; idx < cnt; idx++)
      {
          CFRetain(list[idx]);
      }
      CFArrayRemoveAllValues(timers);
      for (idx = 0; idx < cnt; idx++)
      {
          CFRunLoopTimerRef rlt = (CFRunLoopTimerRef)list[idx];
          __CFRunLoopTimerLock(rlt);
          // if the run loop is deallocating, and since a timer can only be in one
          // run loop, we're going to be removing the timer from all modes, so be
          // a little heavy-handed and direct
          CFSetRemoveAllValues(rlt->_rlModes);
          rlt->_runLoop = NULL;
          __CFRunLoopTimerUnlock(rlt);
          CFRelease(list[idx]);
      }
      if (list != buffer)
          CFAllocatorDeallocate(kCFAllocatorSystemDefault, list);
    };

    if (rlm->_timers && CFArrayGetCount(rlm->_timers))
        deallocateTimers(rlm->_timers);
}

CF_EXPORT CFRunLoopRef _CFRunLoopGet0b(pthread_t t);

static void __CFRunLoopDeallocate(CFTypeRef cf)
{
    CFRunLoopRef rl = (CFRunLoopRef)cf;

    if (_CFRunLoopGet0b(pthread_main_thread_np()) == cf)
        HALT;

    /* We try to keep the run loop in a valid state as long as possible,
       since sources may have non-retained references to the run loop.
       Another reason is that we don't want to lock the run loop for
       callback reasons, if we can get away without that.  We start by
       eliminating the sources, since they are the most likely to call
       back into the run loop during their "cancellation". Common mode
       items will be removed from the mode indirectly by the following
       three lines. */
    __CFRunLoopSetDeallocating(rl);
    if (NULL != rl->_modes)
    {
        CFSetApplyFunction(rl->_modes, (__CFRunLoopCleanseSources), rl); // remove references to rl
        CFSetApplyFunction(rl->_modes, (__CFRunLoopDeallocateSources), rl);
        CFSetApplyFunction(rl->_modes, (__CFRunLoopDeallocateObservers), rl);
        CFSetApplyFunction(rl->_modes, (__CFRunLoopDeallocateTimers), rl);
    }
    __CFRunLoopLock(rl);
    struct _block_item *item = rl->_blocks_head;
    while (item)
    {
        struct _block_item *curr = item;
        item = item->_next;
        CFRelease(curr->_mode);
        Block_release(curr->_block);
        free(curr);
    }
    if (NULL != rl->_commonModeItems)
    {
        CFRelease(rl->_commonModeItems);
    }
    if (NULL != rl->_commonModes)
    {
        CFRelease(rl->_commonModes);
    }
    if (NULL != rl->_modes)
    {
        CFRelease(rl->_modes);
    }
    __CFPortFree(rl->_wakeUpPort);
    rl->_wakeUpPort = CFPORT_NULL;
    __CFRunLoopPopPerRunData(rl, NULL);
    __CFRunLoopUnlock(rl);
    pthread_mutex_destroy(&rl->_lock);
    memset((char *)cf + sizeof(CFRuntimeBase), 0x8C, sizeof(struct __CFRunLoop) - sizeof(CFRuntimeBase));
}

static const CFRuntimeClass __CFRunLoopModeClass = {
    0,
    "CFRunLoopMode",
    NULL, // init
    NULL, // copy
    __CFRunLoopModeDeallocate,
    __CFRunLoopModeEqual,
    __CFRunLoopModeHash,
    NULL, //
    __CFRunLoopModeCopyDescription};

static const CFRuntimeClass __CFRunLoopClass = {
    0,
    "CFRunLoop",
    NULL, // init
    NULL, // copy
    __CFRunLoopDeallocate,
    NULL,
    NULL,
    NULL, //
    __CFRunLoopCopyDescription};

CF_PRIVATE void __CFFinalizeRunLoop(uintptr_t data);

CFTypeID CFRunLoopGetTypeID(void)
{
    static dispatch_once_t initOnce;
    dispatch_once(&initOnce, ^{
      __kCFRunLoopTypeID = _CFRuntimeRegisterClass(&__CFRunLoopClass);
      __kCFRunLoopModeTypeID = _CFRuntimeRegisterClass(&__CFRunLoopModeClass);
    });
    return __kCFRunLoopTypeID;
}

///创建CFRunLoopRef
static CFRunLoopRef __CFRunLoopCreate(pthread_t t)
{
    CFRunLoopRef loop = NULL;
    CFRunLoopModeRef rlm;
    uint32_t size = sizeof(struct __CFRunLoop) - sizeof(CFRuntimeBase);
    loop = (CFRunLoopRef)_CFRuntimeCreateInstance(kCFAllocatorSystemDefault, CFRunLoopGetTypeID(), size, NULL);
    if (NULL == loop)
    {
        return NULL;
    }
    (void)__CFRunLoopPushPerRunData(loop);
    __CFRunLoopLockInit(&loop->_lock);
    loop->_wakeUpPort = __CFPortAllocate();
    if (CFPORT_NULL == loop->_wakeUpPort)
        HALT;
    __CFRunLoopSetIgnoreWakeUps(loop);
    // runloop的 _commonModes 默认添加kCFAllocatorSystemDefault
    loop->_commonModes = CFSetCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeSetCallBacks);
    CFSetAddValue(loop->_commonModes, kCFRunLoopDefaultMode);
    loop->_commonModeItems = NULL;
    loop->_currentMode = NULL;
    // runloop的 modes 默认添加kCFAllocatorSystemDefault
    loop->_modes = CFSetCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeSetCallBacks);
    loop->_blocks_head = NULL;
    loop->_blocks_tail = NULL;
    loop->_counterpart = NULL;
    loop->_pthread = t;
#if DEPLOYMENT_TARGET_WINDOWS
    loop->_winthread = GetCurrentThreadId();
#else
    loop->_winthread = 0;
#endif
    rlm = __CFRunLoopFindMode(loop, kCFRunLoopDefaultMode, true);
    if (NULL != rlm)
        __CFRunLoopModeUnlock(rlm);
    return loop;
}

static CFMutableDictionaryRef __CFRunLoops = NULL;
static CFLock_t loopsLock = CFLockInit;

// should only be called by Foundation
// t==0 is a synonym for "main thread" that always works
CF_EXPORT CFRunLoopRef _CFRunLoopGet0(pthread_t t)
{
    //当传入的t为空时，默认赋值为主线程
    if (pthread_equal(t, kNilPthreadT))
    {
        t = pthread_main_thread_np();
    }
    __CFLock(&loopsLock);
    if (!__CFRunLoops)
    { //创建__CFRunLoops
        __CFUnlock(&loopsLock);
        //创建一个空字典
        CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, NULL, &kCFTypeDictionaryValueCallBacks);
        //创建主线程对应的runloop
        CFRunLoopRef mainLoop = __CFRunLoopCreate(pthread_main_thread_np());
        // key为线程， value为main runloop, 保存在字典中
        CFDictionarySetValue(dict, pthreadPointer(pthread_main_thread_np()), mainLoop);
        //__CFRunLoops赋值为dict
        if (!OSAtomicCompareAndSwapPtrBarrier(NULL, dict, (void *volatile *)&__CFRunLoops))
        { //释放dict
            CFRelease(dict);
        }
        //释放mainloop
        CFRelease(mainLoop);
        __CFLock(&loopsLock);
    }

    //从__CFRunLoops中能够获取线程对应的runloop
    CFRunLoopRef loop = (CFRunLoopRef)CFDictionaryGetValue(__CFRunLoops, pthreadPointer(t));
    __CFUnlock(&loopsLock);
    // __CFRunLoops中未获取到缓存，则去创建一个
    if (!loop)
    {
        //创建newloop
        CFRunLoopRef newLoop = __CFRunLoopCreate(t);
        __CFLock(&loopsLock);
        //再尝试从__CFRunLoops中获取一次，仍找不到则将新创建的runloop保存到__CFRunLoops
        loop = (CFRunLoopRef)CFDictionaryGetValue(c, pthreadPointer(t));
        if (!loop)
        {
            CFDictionarySetValue(__CFRunLoops, pthreadPointer(t), newLoop);
            loop = newLoop;
        }
        // don't release run loops inside the loopsLock, because CFRunLoopDeallocate may end up taking it
        __CFUnlock(&loopsLock);
        CFRelease(newLoop);
    }
    if (pthread_equal(t, pthread_self()))
    {
        //如果t为当前线程，则将loop存到当前线程的共享数据之中，下次直接从tsd中获取
        _CFSetTSD(__CFTSDKeyRunLoop, (void *)loop, NULL);
        if (0 == _CFGetTSD(__CFTSDKeyRunLoopCntr))
        {
            _CFSetTSD(__CFTSDKeyRunLoopCntr, (void *)(PTHREAD_DESTRUCTOR_ITERATIONS - 1), (void (*)(void *))__CFFinalizeRunLoop);
        }
    }
    return loop;
}

// should only be called by Foundation
CFRunLoopRef _CFRunLoopGet0b(pthread_t t)
{
    if (pthread_equal(t, kNilPthreadT))
    {
        t = pthread_main_thread_np();
    }
    __CFLock(&loopsLock);
    CFRunLoopRef loop = NULL;
    if (__CFRunLoops)
    {
        loop = (CFRunLoopRef)CFDictionaryGetValue(__CFRunLoops, pthreadPointer(t));
    }
    __CFUnlock(&loopsLock);
    return loop;
}

static void __CFRunLoopRemoveAllSources(CFRunLoopRef rl, CFStringRef modeName);

// Called for each thread as it exits
CF_PRIVATE void __CFFinalizeRunLoop(uintptr_t data)
{
    CFRunLoopRef rl = NULL;
    if (data <= 1)
    {
        __CFLock(&loopsLock);
        if (__CFRunLoops)
        {
            rl = (CFRunLoopRef)CFDictionaryGetValue(__CFRunLoops, pthreadPointer(pthread_self()));
            if (rl)
                CFRetain(rl);
            CFDictionaryRemoveValue(__CFRunLoops, pthreadPointer(pthread_self()));
        }
        __CFUnlock(&loopsLock);
    }
    else
    {
        _CFSetTSD(__CFTSDKeyRunLoopCntr, (void *)(data - 1), (void (*)(void *))__CFFinalizeRunLoop);
    }
    if (rl && CFRunLoopGetMain() != rl)
    { // protect against cooperative threads
        if (NULL != rl->_counterpart)
        {
            CFRelease(rl->_counterpart);
            rl->_counterpart = NULL;
        }
        // purge all sources before deallocation
        CFArrayRef array = CFRunLoopCopyAllModes(rl);
        for (CFIndex idx = CFArrayGetCount(array); idx--;)
        {
            CFStringRef modeName = (CFStringRef)CFArrayGetValueAtIndex(array, idx);
            __CFRunLoopRemoveAllSources(rl, modeName);
        }
        __CFRunLoopRemoveAllSources(rl, kCFRunLoopCommonModes);
        CFRelease(array);
    }
    if (rl)
        CFRelease(rl);
}

pthread_t _CFRunLoopGet1(CFRunLoopRef rl)
{
    return rl->_pthread;
}

// should only be called by Foundation
CF_EXPORT CFTypeRef _CFRunLoopGet2(CFRunLoopRef rl)
{
    CFTypeRef ret = NULL;
    __CFLock(&loopsLock);
    ret = rl->_counterpart;
    __CFUnlock(&loopsLock);
    return ret;
}

// should only be called by Foundation
CF_EXPORT CFTypeRef _CFRunLoopGet2b(CFRunLoopRef rl)
{
    return rl->_counterpart;
}

#if DEPLOYMENT_TARGET_MACOSX
void _CFRunLoopSetCurrent(CFRunLoopRef rl)
{
    if (pthread_main_np())
        return;
    CFRunLoopRef currentLoop = CFRunLoopGetCurrent();
    if (rl != currentLoop)
    {
        CFRetain(currentLoop); // avoid a deallocation of the currentLoop inside the lock
        __CFLock(&loopsLock);
        if (rl)
        {
            CFDictionarySetValue(__CFRunLoops, pthreadPointer(pthread_self()), rl);
        }
        else
        {
            CFDictionaryRemoveValue(__CFRunLoops, pthreadPointer(pthread_self()));
        }
        __CFUnlock(&loopsLock);
        CFRelease(currentLoop);
        _CFSetTSD(__CFTSDKeyRunLoop, NULL, NULL);
        _CFSetTSD(__CFTSDKeyRunLoopCntr, 0, (void (*)(void *))__CFFinalizeRunLoop);
    }
}
#endif

CFRunLoopRef CFRunLoopGetMain(void)
{
    CHECK_FOR_FORK();
    static CFRunLoopRef __main = NULL; // no retain needed
    if (!__main)
        __main = _CFRunLoopGet0(pthread_main_thread_np()); // no CAS needed
    return __main;
}

CFRunLoopRef CFRunLoopGetCurrent(void)
{
    CHECK_FOR_FORK();
    //先去tsd中去查找缓存，有则返回，没有则继续调用_CFRunLoopGet0()
    CFRunLoopRef rl = (CFRunLoopRef)_CFGetTSD(__CFTSDKeyRunLoop);
    if (rl)
        return rl;
    return _CFRunLoopGet0(pthread_self());
}

CFStringRef CFRunLoopCopyCurrentMode(CFRunLoopRef rl)
{
    CHECK_FOR_FORK();
    CFStringRef result = NULL;
    __CFRunLoopLock(rl);
    if (NULL != rl->_currentMode)
    {
        result = (CFStringRef)CFRetain(rl->_currentMode->_name);
    }
    __CFRunLoopUnlock(rl);
    return result;
}

static void __CFRunLoopGetModeName(const void *value, void *context)
{
    CFRunLoopModeRef rlm = (CFRunLoopModeRef)value;
    CFMutableArrayRef array = (CFMutableArrayRef)context;
    CFArrayAppendValue(array, rlm->_name);
}

CFArrayRef CFRunLoopCopyAllModes(CFRunLoopRef rl)
{
    CHECK_FOR_FORK();
    CFMutableArrayRef array;
    __CFRunLoopLock(rl);
    array = CFArrayCreateMutable(kCFAllocatorSystemDefault, CFSetGetCount(rl->_modes), &kCFTypeArrayCallBacks);
    CFSetApplyFunction(rl->_modes, (__CFRunLoopGetModeName), array);
    __CFRunLoopUnlock(rl);
    return array;
}

static void __CFRunLoopAddItemsToCommonMode(const void *value, void *ctx)
{
    CFTypeRef item = (CFTypeRef)value;
    CFRunLoopRef rl = (CFRunLoopRef)(((CFTypeRef *)ctx)[0]);
    CFStringRef modeName = (CFStringRef)(((CFTypeRef *)ctx)[1]);
    if (CFGetTypeID(item) == CFRunLoopSourceGetTypeID())
    {
        CFRunLoopAddSource(rl, (CFRunLoopSourceRef)item, modeName);
    }
    else if (CFGetTypeID(item) == CFRunLoopObserverGetTypeID())
    {
        CFRunLoopAddObserver(rl, (CFRunLoopObserverRef)item, modeName);
    }
    else if (CFGetTypeID(item) == CFRunLoopTimerGetTypeID())
    {
        CFRunLoopAddTimer(rl, (CFRunLoopTimerRef)item, modeName);
    }
}

static void __CFRunLoopAddItemToCommonModes(const void *value, void *ctx)
{
    CFStringRef modeName = (CFStringRef)value;
    CFRunLoopRef rl = (CFRunLoopRef)(((CFTypeRef *)ctx)[0]);
    CFTypeRef item = (CFTypeRef)(((CFTypeRef *)ctx)[1]);
    if (CFGetTypeID(item) == CFRunLoopSourceGetTypeID())
    {
        CFRunLoopAddSource(rl, (CFRunLoopSourceRef)item, modeName);
    }
    else if (CFGetTypeID(item) == CFRunLoopObserverGetTypeID())
    {
        CFRunLoopAddObserver(rl, (CFRunLoopObserverRef)item, modeName);
    }
    else if (CFGetTypeID(item) == CFRunLoopTimerGetTypeID())
    {
        CFRunLoopAddTimer(rl, (CFRunLoopTimerRef)item, modeName);
    }
}

static void __CFRunLoopRemoveItemFromCommonModes(const void *value, void *ctx)
{
    CFStringRef modeName = (CFStringRef)value;
    CFRunLoopRef rl = (CFRunLoopRef)(((CFTypeRef *)ctx)[0]);
    CFTypeRef item = (CFTypeRef)(((CFTypeRef *)ctx)[1]);
    if (CFGetTypeID(item) == CFRunLoopSourceGetTypeID())
    {
        CFRunLoopRemoveSource(rl, (CFRunLoopSourceRef)item, modeName);
    }
    else if (CFGetTypeID(item) == CFRunLoopObserverGetTypeID())
    {
        CFRunLoopRemoveObserver(rl, (CFRunLoopObserverRef)item, modeName);
    }
    else if (CFGetTypeID(item) == CFRunLoopTimerGetTypeID())
    {
        CFRunLoopRemoveTimer(rl, (CFRunLoopTimerRef)item, modeName);
    }
}

CF_EXPORT Boolean _CFRunLoop01(CFRunLoopRef rl, CFStringRef modeName)
{
    __CFRunLoopLock(rl);
    Boolean present = CFSetContainsValue(rl->_commonModes, modeName);
    __CFRunLoopUnlock(rl);
    return present;
}

void CFRunLoopAddCommonMode(CFRunLoopRef rl, CFStringRef modeName)
{
    CHECK_FOR_FORK();
    if (__CFRunLoopIsDeallocating(rl))
        return;
    __CFRunLoopLock(rl);
    if (!CFSetContainsValue(rl->_commonModes, modeName))
    {
        CFSetRef set = rl->_commonModeItems ? CFSetCreateCopy(kCFAllocatorSystemDefault, rl->_commonModeItems) : NULL;
        CFSetAddValue(rl->_commonModes, modeName);
        if (NULL != set)
        {
            CFTypeRef context[2] = {rl, modeName};
            /* add all common-modes items to new mode */
            CFSetApplyFunction(set, (__CFRunLoopAddItemsToCommonMode), (void *)context);
            CFRelease(set);
        }
    }
    else
    {
    }
    __CFRunLoopUnlock(rl);
}

static void __CFRUNLOOP_IS_SERVICING_THE_MAIN_DISPATCH_QUEUE__() __attribute__((noinline));
static void __CFRUNLOOP_IS_SERVICING_THE_MAIN_DISPATCH_QUEUE__(void *msg)
{
    _dispatch_main_queue_callback_4CF(msg);
    asm __volatile__(""); // thwart tail-call optimization
}

static void __CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__() __attribute__((noinline));
static void __CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__(CFRunLoopObserverCallBack func, CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info)
{
    if (func)
    {
        func(observer, activity, info);
    }
    asm __volatile__(""); // thwart tail-call optimization
}

static void __CFRUNLOOP_IS_CALLING_OUT_TO_A_TIMER_CALLBACK_FUNCTION__() __attribute__((noinline));
static void __CFRUNLOOP_IS_CALLING_OUT_TO_A_TIMER_CALLBACK_FUNCTION__(CFRunLoopTimerCallBack func, CFRunLoopTimerRef timer, void *info)
{
    if (func)
    {
        func(timer, info);
    }
    asm __volatile__(""); // thwart tail-call optimization
}

static void __CFRUNLOOP_IS_CALLING_OUT_TO_A_BLOCK__() __attribute__((noinline));
static void __CFRUNLOOP_IS_CALLING_OUT_TO_A_BLOCK__(void (^block)(void))
{
    if (block)
    {
        block();
    }
    asm __volatile__(""); // thwart tail-call optimization
}

static Boolean __CFRunLoopDoBlocks(CFRunLoopRef rl, CFRunLoopModeRef rlm)
{ // Call with rl and rlm locked
    if (!rl->_blocks_head)
        return false;
    if (!rlm || !rlm->_name)
        return false;
    Boolean did = false;
    struct _block_item *head = rl->_blocks_head;
    struct _block_item *tail = rl->_blocks_tail;
    rl->_blocks_head = NULL;
    rl->_blocks_tail = NULL;
    CFSetRef commonModes = rl->_commonModes;
    CFStringRef curMode = rlm->_name;
    __CFRunLoopModeUnlock(rlm);
    __CFRunLoopUnlock(rl);
    struct _block_item *prev = NULL;
    struct _block_item *item = head;
    while (item)
    {
        struct _block_item *curr = item;
        item = item->_next;
        Boolean doit = false;
        if (CFStringGetTypeID() == CFGetTypeID(curr->_mode))
        {
            doit = CFEqual(curr->_mode, curMode) || (CFEqual(curr->_mode, kCFRunLoopCommonModes) && CFSetContainsValue(commonModes, curMode));
        }
        else
        {
            doit = CFSetContainsValue((CFSetRef)curr->_mode, curMode) || (CFSetContainsValue((CFSetRef)curr->_mode, kCFRunLoopCommonModes) && CFSetContainsValue(commonModes, curMode));
        }
        if (!doit)
            prev = curr;
        if (doit)
        {
            if (prev)
                prev->_next = item;
            if (curr == head)
                head = item;
            if (curr == tail)
                tail = prev;
            void (^block)(void) = curr->_block;
            CFRelease(curr->_mode);
            free(curr);
            if (doit)
            {
                __CFRUNLOOP_IS_CALLING_OUT_TO_A_BLOCK__(block);
                did = true;
            }
            Block_release(block); // do this before relocking to prevent deadlocks where some yahoo wants to run the run loop reentrantly from their dealloc
        }
    }
    __CFRunLoopLock(rl);
    __CFRunLoopModeLock(rlm);
    if (head)
    {
        tail->_next = rl->_blocks_head;
        rl->_blocks_head = head;
        if (!rl->_blocks_tail)
            rl->_blocks_tail = tail;
    }
    return did;
}

/* rl is locked, rlm is locked on entrance and exit */
static void __CFRunLoopDoObservers() __attribute__((noinline));
static void __CFRunLoopDoObservers(CFRunLoopRef rl, CFRunLoopModeRef rlm, CFRunLoopActivity activity)
{ /* DOES CALLOUT */
    CHECK_FOR_FORK();

    CFIndex cnt = rlm->_observers ? CFArrayGetCount(rlm->_observers) : 0;
    if (cnt < 1)
        return;

    /* Fire the observers */
    STACK_BUFFER_DECL(CFRunLoopObserverRef, buffer, (cnt <= 1024) ? cnt : 1);
    CFRunLoopObserverRef *collectedObservers = (cnt <= 1024) ? buffer : (CFRunLoopObserverRef *)malloc(cnt * sizeof(CFRunLoopObserverRef));
    CFIndex obs_cnt = 0;
    for (CFIndex idx = 0; idx < cnt; idx++)
    {
        CFRunLoopObserverRef rlo = (CFRunLoopObserverRef)CFArrayGetValueAtIndex(rlm->_observers, idx);
        if (0 != (rlo->_activities & activity) && __CFIsValid(rlo) && !__CFRunLoopObserverIsFiring(rlo))
        {
            collectedObservers[obs_cnt++] = (CFRunLoopObserverRef)CFRetain(rlo);
        }
    }
    __CFRunLoopModeUnlock(rlm);
    __CFRunLoopUnlock(rl);
    for (CFIndex idx = 0; idx < obs_cnt; idx++)
    {
        CFRunLoopObserverRef rlo = collectedObservers[idx];
        __CFRunLoopObserverLock(rlo);
        if (__CFIsValid(rlo))
        {
            Boolean doInvalidate = !__CFRunLoopObserverRepeats(rlo);
            __CFRunLoopObserverSetFiring(rlo);
            __CFRunLoopObserverUnlock(rlo);
            __CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__(rlo->_callout, rlo, activity, rlo->_context.info);
            if (doInvalidate)
            {
                CFRunLoopObserverInvalidate(rlo);
            }
            __CFRunLoopObserverUnsetFiring(rlo);
        }
        else
        {
            __CFRunLoopObserverUnlock(rlo);
        }
        CFRelease(rlo);
    }
    __CFRunLoopLock(rl);
    __CFRunLoopModeLock(rlm);

    if (collectedObservers != buffer)
        free(collectedObservers);
}

static CFComparisonResult __CFRunLoopSourceComparator(const void *val1, const void *val2, void *context)
{
    CFRunLoopSourceRef o1 = (CFRunLoopSourceRef)val1;
    CFRunLoopSourceRef o2 = (CFRunLoopSourceRef)val2;
    if (o1->_order < o2->_order)
        return kCFCompareLessThan;
    if (o2->_order < o1->_order)
        return kCFCompareGreaterThan;
    return kCFCompareEqualTo;
}

static void __CFRunLoopCollectSources0(const void *value, void *context)
{
    CFRunLoopSourceRef rls = (CFRunLoopSourceRef)value;
    CFTypeRef *sources = (CFTypeRef *)context;
    if (0 == rls->_context.version0.version && __CFIsValid(rls) && __CFRunLoopSourceIsSignaled(rls))
    {
        if (NULL == *sources)
        {
            *sources = CFRetain(rls);
        }
        else if (CFGetTypeID(*sources) == CFRunLoopSourceGetTypeID())
        {
            CFTypeRef oldrls = *sources;
            *sources = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
            CFArrayAppendValue((CFMutableArrayRef)*sources, oldrls);
            CFArrayAppendValue((CFMutableArrayRef)*sources, rls);
            CFRelease(oldrls);
        }
        else
        {
            CFArrayAppendValue((CFMutableArrayRef)*sources, rls);
        }
    }
}

static void __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE0_PERFORM_FUNCTION__() __attribute__((noinline));
static void __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE0_PERFORM_FUNCTION__(void (*perform)(void *), void *info)
{
    if (perform)
    {
        perform(info);
    }
    asm __volatile__(""); // thwart tail-call optimization
}

static void __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE1_PERFORM_FUNCTION__() __attribute__((noinline));
static void __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE1_PERFORM_FUNCTION__(
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
    void *(*perform)(void *msg, CFIndex size, CFAllocatorRef allocator, void *info),
    mach_msg_header_t *msg, CFIndex size, mach_msg_header_t **reply,
#else
    void (*perform)(void *),
#endif
    void *info)
{
    if (perform)
    {
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
        *reply = perform(msg, size, kCFAllocatorSystemDefault, info);
#else
        perform(info);
#endif
    }
    asm __volatile__(""); // thwart tail-call optimization
}

/* rl is locked, rlm is locked on entrance and exit */
static Boolean __CFRunLoopDoSources0(CFRunLoopRef rl, CFRunLoopModeRef rlm, Boolean stopAfterHandle) __attribute__((noinline));
static Boolean __CFRunLoopDoSources0(CFRunLoopRef rl, CFRunLoopModeRef rlm, Boolean stopAfterHandle)
{ /* DOES CALLOUT */
    CHECK_FOR_FORK();
    CFTypeRef sources = NULL;
    Boolean sourceHandled = false;

    /* Fire the version 0 sources */
    if (NULL != rlm->_sources0 && 0 < CFSetGetCount(rlm->_sources0))
    {
        //收集source0
        CFSetApplyFunction(rlm->_sources0, (__CFRunLoopCollectSources0), &sources);
    }
    if (NULL != sources)
    {
        __CFRunLoopModeUnlock(rlm);
        __CFRunLoopUnlock(rl);
        // sources is either a single (retained) CFRunLoopSourceRef or an array of (retained) CFRunLoopSourceRef
        if (CFGetTypeID(sources) == CFRunLoopSourceGetTypeID())
        {
            CFRunLoopSourceRef rls = (CFRunLoopSourceRef)sources;
            __CFRunLoopSourceLock(rls);
            if (__CFRunLoopSourceIsSignaled(rls))
            {
                __CFRunLoopSourceUnsetSignaled(rls);
                if (__CFIsValid(rls))
                {
                    __CFRunLoopSourceUnlock(rls);
                    __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE0_PERFORM_FUNCTION__(rls->_context.version0.perform, rls->_context.version0.info);
                    CHECK_FOR_FORK();
                    sourceHandled = true;
                }
                else
                {
                    __CFRunLoopSourceUnlock(rls);
                }
            }
            else
            {
                __CFRunLoopSourceUnlock(rls);
            }
        }
        else
        {
            CFIndex cnt = CFArrayGetCount((CFArrayRef)sources);
            CFArraySortValues((CFMutableArrayRef)sources, CFRangeMake(0, cnt), (__CFRunLoopSourceComparator), NULL);
            for (CFIndex idx = 0; idx < cnt; idx++)
            {
                CFRunLoopSourceRef rls = (CFRunLoopSourceRef)CFArrayGetValueAtIndex((CFArrayRef)sources, idx);
                __CFRunLoopSourceLock(rls);
                if (__CFRunLoopSourceIsSignaled(rls))
                {
                    __CFRunLoopSourceUnsetSignaled(rls);
                    if (__CFIsValid(rls))
                    {
                        __CFRunLoopSourceUnlock(rls);
                        __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE0_PERFORM_FUNCTION__(rls->_context.version0.perform, rls->_context.version0.info);
                        CHECK_FOR_FORK();
                        sourceHandled = true;
                    }
                    else
                    {
                        __CFRunLoopSourceUnlock(rls);
                    }
                }
                else
                {
                    __CFRunLoopSourceUnlock(rls);
                }
                if (stopAfterHandle && sourceHandled)
                {
                    break;
                }
            }
        }
        CFRelease(sources);
        __CFRunLoopLock(rl);
        __CFRunLoopModeLock(rlm);
    }
    return sourceHandled;
}

CF_INLINE void __CFRunLoopDebugInfoForRunLoopSource(CFRunLoopSourceRef rls)
{
}

// msg, size and reply are unused on Windows
static Boolean __CFRunLoopDoSource1() __attribute__((noinline));
static Boolean __CFRunLoopDoSource1(CFRunLoopRef rl, CFRunLoopModeRef rlm, CFRunLoopSourceRef rls
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
                                    ,
                                    mach_msg_header_t *msg, CFIndex size, mach_msg_header_t **reply
#endif
)
{ /* DOES CALLOUT */
    CHECK_FOR_FORK();
    Boolean sourceHandled = false;

    /* Fire a version 1 source */
    CFRetain(rls);
    __CFRunLoopModeUnlock(rlm);
    __CFRunLoopUnlock(rl);
    __CFRunLoopSourceLock(rls);
    if (__CFIsValid(rls))
    {
        __CFRunLoopSourceUnsetSignaled(rls);
        __CFRunLoopSourceUnlock(rls);
        __CFRunLoopDebugInfoForRunLoopSource(rls);
        __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE1_PERFORM_FUNCTION__(rls->_context.version1.perform,
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
                                                                   msg, size, reply,
#endif
                                                                   rls->_context.version1.info);
        CHECK_FOR_FORK();
        sourceHandled = true;
    }
    else
    {
        if (_LogCFRunLoop)
        {
            CFLog(kCFLogLevelDebug, CFSTR("%p (%s) __CFRunLoopDoSource1 rls %p is invalid"), CFRunLoopGetCurrent(), *_CFGetProgname(), rls);
        }
        __CFRunLoopSourceUnlock(rls);
    }
    CFRelease(rls);
    __CFRunLoopLock(rl);
    __CFRunLoopModeLock(rlm);
    return sourceHandled;
}

static CFIndex __CFRunLoopInsertionIndexInTimerArray(CFArrayRef array, CFRunLoopTimerRef rlt) __attribute__((noinline));
static CFIndex __CFRunLoopInsertionIndexInTimerArray(CFArrayRef array, CFRunLoopTimerRef rlt)
{
    CFIndex cnt = CFArrayGetCount(array);
    if (cnt <= 0)
    {
        return 0;
    }
    if (256 < cnt)
    {
        CFRunLoopTimerRef item = (CFRunLoopTimerRef)CFArrayGetValueAtIndex(array, cnt - 1);
        if (item->_fireTSR <= rlt->_fireTSR)
        {
            return cnt;
        }
        item = (CFRunLoopTimerRef)CFArrayGetValueAtIndex(array, 0);
        if (rlt->_fireTSR < item->_fireTSR)
        {
            return 0;
        }
    }

    CFIndex add = (1 << flsl(cnt)) * 2;
    CFIndex idx = 0;
    Boolean lastTestLEQ;
    do
    {
        add = add / 2;
        lastTestLEQ = false;
        CFIndex testIdx = idx + add;
        if (testIdx < cnt)
        {
            CFRunLoopTimerRef item = (CFRunLoopTimerRef)CFArrayGetValueAtIndex(array, testIdx);
            if (item->_fireTSR <= rlt->_fireTSR)
            {
                idx = testIdx;
                lastTestLEQ = true;
            }
        }
    } while (0 < add);

    return lastTestLEQ ? idx + 1 : idx;
}

static void __CFArmNextTimerInMode(CFRunLoopModeRef rlm, CFRunLoopRef rl)
{
    uint64_t nextHardDeadline = UINT64_MAX;
    uint64_t nextSoftDeadline = UINT64_MAX;

    if (rlm->_timers)
    {
        // Look at the list of timers. We will calculate two TSR values; the next soft and next hard deadline.
        // The next soft deadline is the first time we can fire any timer. This is the fire date of the first timer in our sorted list of timers.
        // The next hard deadline is the last time at which we can fire the timer before we've moved out of the allowable tolerance of the timers in our list.
        for (CFIndex idx = 0, cnt = CFArrayGetCount(rlm->_timers); idx < cnt; idx++)
        {
            CFRunLoopTimerRef t = (CFRunLoopTimerRef)CFArrayGetValueAtIndex(rlm->_timers, idx);
            // discount timers currently firing
            if (__CFRunLoopTimerIsFiring(t))
                continue;

            int32_t err = CHECKINT_NO_ERROR;
            uint64_t oneTimerSoftDeadline = t->_fireTSR;
            uint64_t oneTimerHardDeadline = check_uint64_add(t->_fireTSR, __CFTimeIntervalToTSR(t->_tolerance), &err);
            if (err != CHECKINT_NO_ERROR)
                oneTimerHardDeadline = UINT64_MAX;

            // We can stop searching if the soft deadline for this timer exceeds the current hard deadline. Otherwise, later timers with lower tolerance could still have earlier hard deadlines.
            if (oneTimerSoftDeadline > nextHardDeadline)
            {
                break;
            }

            if (oneTimerSoftDeadline < nextSoftDeadline)
            {
                nextSoftDeadline = oneTimerSoftDeadline;
            }

            if (oneTimerHardDeadline < nextHardDeadline)
            {
                nextHardDeadline = oneTimerHardDeadline;
            }
        }

        if (nextSoftDeadline < UINT64_MAX && (nextHardDeadline != rlm->_timerHardDeadline || nextSoftDeadline != rlm->_timerSoftDeadline))
        {
            if (CFRUNLOOP_NEXT_TIMER_ARMED_ENABLED())
            {
                CFRUNLOOP_NEXT_TIMER_ARMED((unsigned long)(nextSoftDeadline - mach_absolute_time()));
            }
#if USE_DISPATCH_SOURCE_FOR_TIMERS
            // We're going to hand off the range of allowable timer fire date to dispatch and let it fire when appropriate for the system.
            uint64_t leeway = __CFTSRToNanoseconds(nextHardDeadline - nextSoftDeadline);
            dispatch_time_t deadline = __CFTSRToDispatchTime(nextSoftDeadline);
#if USE_MK_TIMER_TOO
            if (leeway > 0)
            {
                // Only use the dispatch timer if we have any leeway
                // <rdar://problem/14447675>

                // Cancel the mk timer
                if (rlm->_mkTimerArmed && rlm->_timerPort)
                {
                    AbsoluteTime dummy;
                    mk_timer_cancel(rlm->_timerPort, &dummy);
                    rlm->_mkTimerArmed = false;
                }

                // Arm the dispatch timer
                _dispatch_source_set_runloop_timer_4CF(rlm->_timerSource, deadline, DISPATCH_TIME_FOREVER, leeway);
                rlm->_dispatchTimerArmed = true;
            }
            else
            {
                // Cancel the dispatch timer
                if (rlm->_dispatchTimerArmed)
                {
                    // Cancel the dispatch timer
                    _dispatch_source_set_runloop_timer_4CF(rlm->_timerSource, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 888);
                    rlm->_dispatchTimerArmed = false;
                }

                // Arm the mk timer
                if (rlm->_timerPort)
                {
                    mk_timer_arm(rlm->_timerPort, __CFUInt64ToAbsoluteTime(nextSoftDeadline));
                    rlm->_mkTimerArmed = true;
                }
            }
#else
            _dispatch_source_set_runloop_timer_4CF(rlm->_timerSource, deadline, DISPATCH_TIME_FOREVER, leeway);
#endif
#else
            if (rlm->_timerPort)
            {
                mk_timer_arm(rlm->_timerPort, __CFUInt64ToAbsoluteTime(nextSoftDeadline));
            }
#endif
        }
        else if (nextSoftDeadline == UINT64_MAX)
        {
            // Disarm the timers - there is no timer scheduled

            if (rlm->_mkTimerArmed && rlm->_timerPort)
            {
                AbsoluteTime dummy;
                mk_timer_cancel(rlm->_timerPort, &dummy);
                rlm->_mkTimerArmed = false;
            }

#if USE_DISPATCH_SOURCE_FOR_TIMERS
            if (rlm->_dispatchTimerArmed)
            {
                _dispatch_source_set_runloop_timer_4CF(rlm->_timerSource, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 333);
                rlm->_dispatchTimerArmed = false;
            }
#endif
        }
    }
    rlm->_timerHardDeadline = nextHardDeadline;
    rlm->_timerSoftDeadline = nextSoftDeadline;
}

// call with rlm and its run loop locked, and the TSRLock locked; rlt not locked; returns with same state
static void __CFRepositionTimerInMode(CFRunLoopModeRef rlm, CFRunLoopTimerRef rlt, Boolean isInArray) __attribute__((noinline));
static void __CFRepositionTimerInMode(CFRunLoopModeRef rlm, CFRunLoopTimerRef rlt, Boolean isInArray)
{
    if (!rlt)
        return;

    CFMutableArrayRef timerArray = rlm->_timers;
    if (!timerArray)
        return;
    Boolean found = false;

    // If we know in advance that the timer is not in the array (just being added now) then we can skip this search
    if (isInArray)
    {
        CFIndex idx = CFArrayGetFirstIndexOfValue(timerArray, CFRangeMake(0, CFArrayGetCount(timerArray)), rlt);
        if (kCFNotFound != idx)
        {
            CFRetain(rlt);
            CFArrayRemoveValueAtIndex(timerArray, idx);
            found = true;
        }
    }
    if (!found && isInArray)
        return;
    CFIndex newIdx = __CFRunLoopInsertionIndexInTimerArray(timerArray, rlt);
    CFArrayInsertValueAtIndex(timerArray, newIdx, rlt);
    __CFArmNextTimerInMode(rlm, rlt->_runLoop);
    if (isInArray)
        CFRelease(rlt);
}

// mode and rl are locked on entry and exit
static Boolean __CFRunLoopDoTimer(CFRunLoopRef rl, CFRunLoopModeRef rlm, CFRunLoopTimerRef rlt)
{ /* DOES CALLOUT */
    Boolean timerHandled = false;
    uint64_t oldFireTSR = 0;

    /* Fire a timer */
    CFRetain(rlt);
    __CFRunLoopTimerLock(rlt);

    if (__CFIsValid(rlt) && rlt->_fireTSR <= mach_absolute_time() && !__CFRunLoopTimerIsFiring(rlt) && rlt->_runLoop == rl)
    {
        void *context_info = NULL;
        void (*context_release)(const void *) = NULL;
        if (rlt->_context.retain)
        {
            context_info = (void *)rlt->_context.retain(rlt->_context.info);
            context_release = rlt->_context.release;
        }
        else
        {
            context_info = rlt->_context.info;
        }
        Boolean doInvalidate = (0.0 == rlt->_interval);
        __CFRunLoopTimerSetFiring(rlt);
        // Just in case the next timer has exactly the same deadlines as this one, we reset these values so that the arm next timer code can correctly find the next timer in the list and arm the underlying timer.
        rlm->_timerSoftDeadline = UINT64_MAX;
        rlm->_timerHardDeadline = UINT64_MAX;
        __CFRunLoopTimerUnlock(rlt);
        __CFRunLoopTimerFireTSRLock();
        oldFireTSR = rlt->_fireTSR;
        __CFRunLoopTimerFireTSRUnlock();

        __CFArmNextTimerInMode(rlm, rl);

        __CFRunLoopModeUnlock(rlm);
        __CFRunLoopUnlock(rl);
        __CFRUNLOOP_IS_CALLING_OUT_TO_A_TIMER_CALLBACK_FUNCTION__(rlt->_callout, rlt, context_info);
        CHECK_FOR_FORK();
        if (doInvalidate)
        {
            CFRunLoopTimerInvalidate(rlt); /* DOES CALLOUT */
        }
        if (context_release)
        {
            context_release(context_info);
        }
        __CFRunLoopLock(rl);
        __CFRunLoopModeLock(rlm);
        __CFRunLoopTimerLock(rlt);
        timerHandled = true;
        __CFRunLoopTimerUnsetFiring(rlt);
    }
    if (__CFIsValid(rlt) && timerHandled)
    {
        /* This is just a little bit tricky: we want to support calling
         * CFRunLoopTimerSetNextFireDate() from within the callout and
         * honor that new time here if it is a later date, otherwise
         * it is completely ignored. */
        if (oldFireTSR < rlt->_fireTSR)
        {
            /* Next fire TSR was set, and set to a date after the previous
             * fire date, so we honor it. */
            __CFRunLoopTimerUnlock(rlt);
            // The timer was adjusted and repositioned, during the
            // callout, but if it was still the min timer, it was
            // skipped because it was firing.  Need to redo the
            // min timer calculation in case rlt should now be that
            // timer instead of whatever was chosen.
            __CFArmNextTimerInMode(rlm, rl);
        }
        else
        {
            uint64_t nextFireTSR = 0LL;
            uint64_t intervalTSR = 0LL;
            if (rlt->_interval <= 0.0)
            {
            }
            else if (TIMER_INTERVAL_LIMIT < rlt->_interval)
            {
                intervalTSR = __CFTimeIntervalToTSR(TIMER_INTERVAL_LIMIT);
            }
            else
            {
                intervalTSR = __CFTimeIntervalToTSR(rlt->_interval);
            }
            if (LLONG_MAX - intervalTSR <= oldFireTSR)
            {
                nextFireTSR = LLONG_MAX;
            }
            else
            {
                if (intervalTSR == 0)
                {
                    // 15304159: Make sure we don't accidentally loop forever here
                    CRSetCrashLogMessage("A CFRunLoopTimer with an interval of 0 is set to repeat");
                    HALT;
                }
                uint64_t currentTSR = mach_absolute_time();
                nextFireTSR = oldFireTSR;
                while (nextFireTSR <= currentTSR)
                {
                    nextFireTSR += intervalTSR;
                }
            }
            CFRunLoopRef rlt_rl = rlt->_runLoop;
            if (rlt_rl)
            {
                CFRetain(rlt_rl);
                CFIndex cnt = CFSetGetCount(rlt->_rlModes);
                STACK_BUFFER_DECL(CFTypeRef, modes, cnt);
                CFSetGetValues(rlt->_rlModes, (const void **)modes);
                // To avoid A->B, B->A lock ordering issues when coming up
                // towards the run loop from a source, the timer has to be
                // unlocked, which means we have to protect from object
                // invalidation, although that's somewhat expensive.
                for (CFIndex idx = 0; idx < cnt; idx++)
                {
                    CFRetain(modes[idx]);
                }
                __CFRunLoopTimerUnlock(rlt);
                for (CFIndex idx = 0; idx < cnt; idx++)
                {
                    CFStringRef name = (CFStringRef)modes[idx];
                    modes[idx] = (CFTypeRef)__CFRunLoopFindMode(rlt_rl, name, false);
                    CFRelease(name);
                }
                __CFRunLoopTimerFireTSRLock();
                rlt->_fireTSR = nextFireTSR;
                rlt->_nextFireDate = CFAbsoluteTimeGetCurrent() + __CFTimeIntervalUntilTSR(nextFireTSR);
                for (CFIndex idx = 0; idx < cnt; idx++)
                {
                    CFRunLoopModeRef rlm = (CFRunLoopModeRef)modes[idx];
                    if (rlm)
                    {
                        __CFRepositionTimerInMode(rlm, rlt, true);
                    }
                }
                __CFRunLoopTimerFireTSRUnlock();
                for (CFIndex idx = 0; idx < cnt; idx++)
                {
                    __CFRunLoopModeUnlock((CFRunLoopModeRef)modes[idx]);
                }
                CFRelease(rlt_rl);
            }
            else
            {
                __CFRunLoopTimerUnlock(rlt);
                __CFRunLoopTimerFireTSRLock();
                rlt->_fireTSR = nextFireTSR;
                rlt->_nextFireDate = CFAbsoluteTimeGetCurrent() + __CFTimeIntervalUntilTSR(nextFireTSR);
                __CFRunLoopTimerFireTSRUnlock();
            }
        }
    }
    else
    {
        __CFRunLoopTimerUnlock(rlt);
    }
    CFRelease(rlt);
    return timerHandled;
}

// rl and rlm are locked on entry and exit
static Boolean __CFRunLoopDoTimers(CFRunLoopRef rl, CFRunLoopModeRef rlm, uint64_t limitTSR)
{ /* DOES CALLOUT */
    Boolean timerHandled = false;
    CFMutableArrayRef timers = NULL;
    for (CFIndex idx = 0, cnt = rlm->_timers ? CFArrayGetCount(rlm->_timers) : 0; idx < cnt; idx++)
    {
        CFRunLoopTimerRef rlt = (CFRunLoopTimerRef)CFArrayGetValueAtIndex(rlm->_timers, idx);

        if (__CFIsValid(rlt) && !__CFRunLoopTimerIsFiring(rlt))
        {
            if (rlt->_fireTSR <= limitTSR)
            {
                if (!timers)
                    timers = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
                CFArrayAppendValue(timers, rlt);
            }
        }
    }

    for (CFIndex idx = 0, cnt = timers ? CFArrayGetCount(timers) : 0; idx < cnt; idx++)
    {
        CFRunLoopTimerRef rlt = (CFRunLoopTimerRef)CFArrayGetValueAtIndex(timers, idx);
        Boolean did = __CFRunLoopDoTimer(rl, rlm, rlt);
        timerHandled = timerHandled || did;
    }
    if (timers)
        CFRelease(timers);
    return timerHandled;
}

CF_EXPORT Boolean _CFRunLoopFinished(CFRunLoopRef rl, CFStringRef modeName)
{
    CHECK_FOR_FORK();
    CFRunLoopModeRef rlm;
    Boolean result = false;
    __CFRunLoopLock(rl);
    rlm = __CFRunLoopFindMode(rl, modeName, false);
    if (NULL == rlm || __CFRunLoopModeIsEmpty(rl, rlm, NULL))
    {
        result = true;
    }
    if (rlm)
        __CFRunLoopModeUnlock(rlm);
    __CFRunLoopUnlock(rl);
    return result;
}

static int32_t __CFRunLoopRun(CFRunLoopRef rl, CFRunLoopModeRef rlm, CFTimeInterval seconds, Boolean stopAfterHandle, CFRunLoopModeRef previousMode) __attribute__((noinline));

#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI

#define TIMEOUT_INFINITY (~(mach_msg_timeout_t)0)

static Boolean __CFRunLoopServiceMachPort(mach_port_name_t port, mach_msg_header_t **buffer, size_t buffer_size, mach_port_t *livePort, mach_msg_timeout_t timeout, voucher_mach_msg_state_t *voucherState, voucher_t *voucherCopy)
{
    Boolean originalBuffer = true;
    kern_return_t ret = KERN_SUCCESS;
    for (;;)
    { /* In that sleep of death what nightmares may come ... */
        mach_msg_header_t *msg = (mach_msg_header_t *)*buffer;
        msg->msgh_bits = 0;
        msg->msgh_local_port = port;
        msg->msgh_remote_port = MACH_PORT_NULL;
        msg->msgh_size = buffer_size;
        msg->msgh_id = 0;
        if (TIMEOUT_INFINITY == timeout)
        {
            CFRUNLOOP_SLEEP();
        }
        else
        {
            CFRUNLOOP_POLL();
        }
        ret = mach_msg(msg, MACH_RCV_MSG | (voucherState ? MACH_RCV_VOUCHER : 0) | MACH_RCV_LARGE | ((TIMEOUT_INFINITY != timeout) ? MACH_RCV_TIMEOUT : 0) | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) | MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AV), 0, msg->msgh_size, port, timeout, MACH_PORT_NULL);

        // Take care of all voucher-related work right after mach_msg.
        // If we don't release the previous voucher we're going to leak it.
        voucher_mach_msg_revert(*voucherState);

        // Someone will be responsible for calling voucher_mach_msg_revert. This call makes the received voucher the current one.
        *voucherState = voucher_mach_msg_adopt(msg);

        if (voucherCopy)
        {
            if (*voucherState != VOUCHER_MACH_MSG_STATE_UNCHANGED)
            {
                // Caller requested a copy of the voucher at this point. By doing this right next to mach_msg we make sure that no voucher has been set in between the return of mach_msg and the use of the voucher copy.
                // CFMachPortBoost uses the voucher to drop importance explicitly. However, we want to make sure we only drop importance for a new voucher (not unchanged), so we only set the TSD when the voucher is not state_unchanged.
                *voucherCopy = voucher_copy();
            }
            else
            {
                *voucherCopy = NULL;
            }
        }

        CFRUNLOOP_WAKEUP(ret);
        if (MACH_MSG_SUCCESS == ret)
        {
            *livePort = msg ? msg->msgh_local_port : MACH_PORT_NULL;
            return true;
        }
        if (MACH_RCV_TIMED_OUT == ret)
        {
            if (!originalBuffer)
                free(msg);
            *buffer = NULL;
            *livePort = MACH_PORT_NULL;
            return false;
        }
        if (MACH_RCV_TOO_LARGE != ret)
            break;
        buffer_size = round_msg(msg->msgh_size + MAX_TRAILER_SIZE);
        if (originalBuffer)
            *buffer = NULL;
        originalBuffer = false;
        *buffer = realloc(*buffer, buffer_size);
    }
    HALT;
    return false;
}

#elif DEPLOYMENT_TARGET_WINDOWS

#define TIMEOUT_INFINITY INFINITE

// pass in either a portSet or onePort
static Boolean __CFRunLoopWaitForMultipleObjects(__CFPortSet portSet, HANDLE *onePort, DWORD timeout, DWORD mask, HANDLE *livePort, Boolean *msgReceived)
{
    DWORD waitResult = WAIT_TIMEOUT;
    HANDLE handleBuf[MAXIMUM_WAIT_OBJECTS];
    HANDLE *handles = NULL;
    uint32_t handleCount = 0;
    Boolean freeHandles = false;
    Boolean result = false;

    if (portSet)
    {
        // copy out the handles to be safe from other threads at work
        handles = __CFPortSetGetPorts(portSet, handleBuf, MAXIMUM_WAIT_OBJECTS, &handleCount);
        freeHandles = (handles != handleBuf);
    }
    else
    {
        handles = onePort;
        handleCount = 1;
        freeHandles = FALSE;
    }

    // The run loop mode and loop are already in proper unlocked state from caller
    waitResult = MsgWaitForMultipleObjectsEx(__CFMin(handleCount, MAXIMUM_WAIT_OBJECTS), handles, timeout, mask, MWMO_INPUTAVAILABLE);

    CFAssert2(waitResult != WAIT_FAILED, __kCFLogAssertion, "%s(): error %d from MsgWaitForMultipleObjects", __PRETTY_FUNCTION__, GetLastError());

    if (waitResult == WAIT_TIMEOUT)
    {
        // do nothing, just return to caller
        result = false;
    }
    else if (waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + handleCount)
    {
        // a handle was signaled
        if (livePort)
            *livePort = handles[waitResult - WAIT_OBJECT_0];
        result = true;
    }
    else if (waitResult == WAIT_OBJECT_0 + handleCount)
    {
        // windows message received
        if (msgReceived)
            *msgReceived = true;
        result = true;
    }
    else if (waitResult >= WAIT_ABANDONED_0 && waitResult < WAIT_ABANDONED_0 + handleCount)
    {
        // an "abandoned mutex object"
        if (livePort)
            *livePort = handles[waitResult - WAIT_ABANDONED_0];
        result = true;
    }
    else
    {
        CFAssert2(waitResult == WAIT_FAILED, __kCFLogAssertion, "%s(): unexpected result from MsgWaitForMultipleObjects: %d", __PRETTY_FUNCTION__, waitResult);
        result = false;
    }

    if (freeHandles)
    {
        CFAllocatorDeallocate(kCFAllocatorSystemDefault, handles);
    }

    return result;
}

#endif

struct __timeout_context
{
    dispatch_source_t ds;
    CFRunLoopRef rl;
    uint64_t termTSR;
};

static void __CFRunLoopTimeoutCancel(void *arg)
{
    struct __timeout_context *context = (struct __timeout_context *)arg;
    CFRelease(context->rl);
    dispatch_release(context->ds);
    free(context);
}

static void __CFRunLoopTimeout(void *arg)
{
    struct __timeout_context *context = (struct __timeout_context *)arg;
    context->termTSR = 0ULL;
    CFRUNLOOP_WAKEUP_FOR_TIMEOUT();
    CFRunLoopWakeUp(context->rl);
    // The interval is DISPATCH_TIME_FOREVER, so this won't fire again
}

/* rl, rlm are locked on entrance and exit */
/// run loop 真正的运行函数
static int32_t __CFRunLoopRun(CFRunLoopRef rl,
                              CFRunLoopModeRef rlm,
                              CFTimeInterval seconds,  // 1e10
                              Boolean stopAfterHandle, // false
                              CFRunLoopModeRef previousMode)
{
    // mach_absolute_time 返回一个基于系统启动后的时钟嘀嗒数，是一个 CPU/总线 依赖函数。
    // 在 macOS 上可以确保它的行为，并且它包含系统时钟所拥有的全部时间区域，精度达到纳秒级。
    // 时钟嘀嗒数在每次手机重启后，都会重新开始计数，而且 iPhone 锁屏进入休眠之后，tick 也会暂停计数
    uint64_t startTSR = mach_absolute_time();

    // 判断 rl 是否已停止，(rl->_perRunData->stopped) ? true : false;
    // rl->_perRunData->stopped 的值为 0x53544F50/0x0，
    // 当值是 0x53544F50 时表示 rl 已经停止，是 0x0 时表示未设置停止标记即非停止状态。
    if (__CFRunLoopIsStopped(rl))
    {
        // 设置 rl->_perRunData->stopped = 0x0 表示未设置停止标记的状态，即表示 rl 是非停止状态。
        __CFRunLoopUnsetStopped(rl);
        // 然后直接返回 kCFRunLoopRunStopped
        return kCFRunLoopRunStopped;
    }
    else if (rlm->_stopped)
    {
        // 如果 _stopped 是 true，则把 _stopped 置为 false，表示 flm 是非停止状态。
        rlm->_stopped = false;
        // 然后直接返回 kCFRunLoopRunStopped
        return kCFRunLoopRunStopped;
    }
    // 声明一个 mach_port_name_t 类型的局部变量 dispatchPort，用于记录主队列的端口，只有当前处于主线程时下面才会被赋值为主队列的端口。
    // unsigned int
    // #define MACH_PORT_NULL   0
    mach_port_name_t dispatchPort = MACH_PORT_NULL;

    // #define HANDLE_DISPATCH_ON_BASE_INVOCATION_ONLY 0
    // 当前是主线程并且从当前线程的 TSD 中获取 __CFTSDKeyIsInGCDMainQ 得到的是 0 的话 libdispatchQSafe 的值才会为 true。
    Boolean libdispatchQSafe = pthread_main_np() && ((HANDLE_DISPATCH_ON_BASE_INVOCATION_ONLY && NULL == previousMode) || (!HANDLE_DISPATCH_ON_BASE_INVOCATION_ONLY && 0 == _CFGetTSD(__CFTSDKeyIsInGCDMainQ)));
    // 1. libdispatchQSafe 为真
    // 2. 入参 rl 是 main run loop
    // 3. 入参 rlm->_name 被 rl->_commonModes 包含
    // 以上三个条件都是真的话，则把主线程主队列的端口号赋值给 dispatchPort 变量，
    // 用于当使用 dispatch_async 添加到主队列任务时唤醒 main run loop 执行。
    if (libdispatchQSafe && (CFRunLoopGetMain() == rl) && CFSetContainsValue(rl->_commonModes, rlm->_name))
        dispatchPort = _dispatch_get_main_queue_port_4CF();

    //USE_DISPATCH_SOURCE_FOR_TIMERS==0
#if USE_DISPATCH_SOURCE_FOR_TIMERS
    // 在 rlm 中使用 dispatch_source 构建的 timer
    mach_port_name_t modeQueuePort = MACH_PORT_NULL;

    // run loop mode 创建时，会对 _queue 字段赋初值
    // rlm->_queue = _dispatch_runloop_root_queue_create_4CF("Run Loop Mode Queue", 0);

    if (rlm->_queue)
    {
        // 获取 rlm->_queue 的 port
        modeQueuePort = _dispatch_runloop_root_queue_get_port_4CF(rlm->_queue);
        if (!modeQueuePort)
        { // 如果获取端口失败，则 carsh 描述信息是：无法获取运行循环模式队列的端口
            CRASH("Unable to get port for run loop mode queue (%d)", -1);
        }
    }
#endif
    // GCD timer 是依赖于内核的，所以非常精准，不受 run loop 影响。

    // 由 dispatch_suorce 构建计时器
    dispatch_source_t timeout_timer = NULL;
    // 为计时器参数 timeout_context 申请内存空间
    struct __timeout_context *timeout_context = (struct __timeout_context *)malloc(sizeof(*timeout_context));
    if (seconds <= 0.0)
    { // instant timeout 立即超时
        // 如果 run loop 运行时间 seconds 参数小于等于 0.0，则立即超时
        seconds = 0.0;
        timeout_context->termTSR = 0ULL;
    }
    else if (seconds <= TIMER_INTERVAL_LIMIT)
    {
        // 大于 0.0 小于 504911232.0 的 run loop 运行时间，其它情况的 seconds 的话都表示永不超时
        // #define TIMER_INTERVAL_LIMIT   504911232.0

        // 如果当前是主线程，则 queue = dispatch_get_global_queue(qos_class_main(), DISPATCH_QUEUE_OVERCOMMIT)，
        // 否则 queue = dispatch_get_global_queue(QOS_CLASS_UTILITY, DISPATCH_QUEUE_OVERCOMMIT)。
        // 当 DEPLOYMENT_TARGET_IPHONESIMULATOR 下运行时，#define qos_class_main() (QOS_CLASS_UTILITY)，
        // 即不管当前是主线程还是子线程，queue 都表示是一个全局并发队列。
        // 这个队列主要用来执行 run loop 的休眠的计时器用的，所以理论上只要是一个主队列之外的并发队列即可。
        dispatch_queue_t queue = pthread_main_np() ? __CFDispatchQueueGetGenericMatchingMain() : __CFDispatchQueueGetGenericBackground();
        // 指定 dispatch_source 为 DISPATCH_SOURCE_TYPE_TIMER 类型，即构建一个计时器类型的 dispatch_source，赋值给 timeout_timer
        timeout_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
        dispatch_retain(timeout_timer);

        // 设置 __timeout_context 的三个成员变量
        //（在 __CFRunLoopTimeoutCancel 回调函数中 __timeout_context 以及其 rl、ds 成员变量会进行释放）

        // ds 是 dispatch_source 的首字母缩写，
        //（timer 的回调参数 timeout_context 会携带 timeout_timer 计时器本身）
        timeout_context->ds = timeout_timer;
        // timeout_context 持有 rl，
        //（timer 的回调参数 timeout_context 会携带 timeout_timer 计时器所处的 run loop）
        timeout_context->rl = (CFRunLoopRef)CFRetain(rl);
        // termTSR 是当前时间加上 run loop 运行时间的参数 seconds
        timeout_context->termTSR = startTSR + __CFTimeIntervalToTSR(seconds);
        // 设置 timeout_timer 计数器的上下文，即计时器回调函数的参数
        dispatch_set_context(timeout_timer, timeout_context);
        // source gets ownership of context
        // 设置 timeout_timer 计时器的执行的回调函数 __CFRunLoopTimeout
        // __CFRunLoopTimeout 函数内部会调用 CFRunLoopWakeUp(context->rl) 唤醒 timeout_context->rl
        //（可跳到下面先看一下 __CFRunLoopTimeout 函数实现）
        dispatch_source_set_event_handler_f(timeout_timer, __CFRunLoopTimeout);
        // 设置 timeout_timer 计时器取消时的回调函数，对 timeout_timer 调用 dispatch_source_cancel 函数后，会触发此回调
        //（可跳到下面先看一下 __CFRunLoopTimeoutCancel 函数实现）
        dispatch_source_set_cancel_handler_f(timeout_timer, __CFRunLoopTimeoutCancel);
        // 换算秒数
        // * 1000000000ULL 是把纳秒转化为秒
        uint64_t ns_at = (uint64_t)((__CFTSRToTimeInterval(startTSR) + seconds) * 1000000000ULL);
        // 计时器 timeout_timer 的执行时间间隔是 DISPATCH_TIME_FOREVER，第一次触发时间是 dispatch_time(1, ns_at) 后
        //（时间间隔为 DISPATCH_TIME_FOREVER，因此不会再次触发）
        dispatch_source_set_timer(timeout_timer, dispatch_time(1, ns_at), DISPATCH_TIME_FOREVER, 1000ULL);
        // dispatch_resume 恢复调度对象上块的调用，这里的作用是启动 timeout_timer 计时器
        dispatch_resume(timeout_timer);
    }
    else
    { // infinite timeout 永不超时
        seconds = 9999999999.0;
        timeout_context->termTSR = UINT64_MAX;
    }

    Boolean didDispatchPortLastTime = true;

    // run loop run 返回值，默认为 0，会在 do while 中根据情况被修改，
    // 当不为 0 时，run loop 退出，如果 do while 结尾处 retVal 还是 0 则继续循环。
    int32_t retVal = 0;
    // 这个 do while 循环超长几乎包含了剩下的所有函数内容，
    // 中间还嵌套了一个较短的 do while 循环用于处理 run loop 的休眠和唤醒。
    // dododododododododododododododododododododododododododododododododododododododododododododo
    do
    {
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
        // macOS 下
        voucher_mach_msg_state_t voucherState = VOUCHER_MACH_MSG_STATE_UNCHANGED;
        voucher_t voucherCopy = NULL;
#endif
        // 3072
        uint8_t msg_buffer[3 * 1024];
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
        mach_msg_header_t *msg = NULL;
        mach_port_t livePort = MACH_PORT_NULL;
#elif DEPLOYMENT_TARGET_WINDOWS
        HANDLE livePort = NULL;
        Boolean windowsMessageReceived = false;
#endif
        // 取当前 rlm 所需要监听的 mach port 集合，用于唤醒 run loop（__CFPortSet 实际上是 unsigned int 类型）
        __CFPortSet waitSet = rlm->_portSet;
        // 设置 rl->_perRunData->ignoreWakeUps = 0x0，表示未设置 IgnoreWakeUps 标记位。
        // rl->_perRunData->ignoreWakeUps = 0x57414B45/0x0，当值是 0x57414B45 时表示设置为 "忽略唤醒" 标记（IgnoreWakeUps），
        // 当值是 0x0 时表示未设置 "忽略唤醒"，此时 CFRunLoopWakeUp 函数才能正常唤醒 run loop，否则会直接 return。
        // Unset
        __CFRunLoopUnsetIgnoreWakeUps(rl);

        // kCFRunLoopBeforeTimers（处理 timer 前） 和 kCFRunLoopBeforeSources（处理 source0 前)
        //当前所有的 observer 进行 run loop 活动状态回调
        if (rlm->_observerMask & kCFRunLoopBeforeTimers)
            // 通知即将处理 Timers
            __CFRunLoopDoObservers(rl, rlm, kCFRunLoopBeforeTimers);
        if (rlm->_observerMask & kCFRunLoopBeforeSources)
            // 通知即将处理 Sources
            __CFRunLoopDoObservers(rl, rlm, kCFRunLoopBeforeSources);

        // 遍历 rl 的 block 链表中的可在当前 run loop 运行模式下执行的 block，执行它们，执行完会把它们从链表中移除，并调用 Block_release 函数释放，
        // 得不到执行的 block 则继续留在链表中，等待 run loop 切换到 block 适合的 run loop mode 时再执行，
        // 会首先把 rl 的 _blocks_head 和 _blocks_tail 置为 NULL，然后得到执行的 block 执行完毕后会从链表中移除并调用 Block_release 函数。
        //（block 执行时调用的是 __CFRUNLOOP_IS_CALLING_OUT_TO_A_BLOCK__ 函数）
        //（我们开始收集这种名字大写的函数，在 run loop 学习过程中我们会遇到多个这种命名方式的函数，当我们都收集完了，那么 run loop 的学习就很熟悉了）

        // 目前我们收集到两个：
        // __CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__ // run loop 的状态发生变化前执行 run loop observer 的回调函数
        // __CFRUNLOOP_IS_CALLING_OUT_TO_A_BLOCK__ // 执行 run loop 的 block 链表中的 block（在 run loop 当前运行模式下可执行的链表中的 block，
        //                                            会得到执行，执行完以后会被释放并移除，不能在此模式下执行的 block 则还会保留在 block 链表中）

        //（可跳到下面先看一下 __CFRunLoopDoBlocks 函数实现）
        // 处理 Blocks
        __CFRunLoopDoBlocks(rl, rlm);

        // 执行 rlm 的 _sources0 集合中的 Valid 和 Signaled 的 source，（执行 CFRunLoopSourceRef 的 perform 回调函数）

        // 目前我们收集到三个：
        // __CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__ // run loop 的状态发生变化前执行 run loop observer 的回调函数
        // __CFRUNLOOP_IS_CALLING_OUT_TO_A_BLOCK__ // 执行 run loop 的 block 链表中的 block（block 执行完以后会被释放并移除）
        // __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE0_PERFORM_FUNCTION__ // 执行 run loop mode 的 _source0 中的 CFRunLoopSourceRef 的 perfom 函数（以其 info 为参数）

        // 遍历 rlm 的 _source0 中的 Valid 和 Signaled 的 CFRunLoopSourceRef，执行其 perform 函数（perform(info)），
        // 且要执行的 CFRunLoopSourceRef 会被置为 UnsetSignaled，
        // 那么下次 run loop 循环便不再执行这个 UnsetSignaled 的 CFRunLoopSourceRef 了。
        // 当有执行 source0 的 perform 函数时则返回 true，否则返回 false。
        // 处理 Source0
        Boolean sourceHandledThisLoop = __CFRunLoopDoSources0(rl, rlm, stopAfterHandle);
        // sourceHandledThisLoop 的值表示 __CFRunLoopDoSources0 函数内部是否对 rlm 的 _sources0 中的 CFRunLoopSourceRef 执行了它的 void (*perform)(void *info) 函数。

        // 如果为真则再次遍历 rl 的 block 链表中的在指定 rlm 下执行的 block，
        //（这里没看出来 __CFRunLoopDoSources0 和 rl 的 block 链表有啥联系呀，为什么又执行链表 block 呢？难道 source0 执行会改变 run loop 的运行模式吗？也不对呀，这里入参还是 rl 和 rlm）
        if (sourceHandledThisLoop)
        {
            __CFRunLoopDoBlocks(rl, rlm);
        }

        // 如果 rlm 的 _sources0 中有 CFRunLoopSourceRef 执行了 perform 函数 或者 timeout_context->termTSR 等于 0，则 poll 的值为 true 否则为 false。
        // timeout_context->termTSR == 0ULL 的情况：
        // 1. 当 seconds 入参小于等于 0 时（入参的 run loop 运行时间小于等于 0）
        // 2. 当👆创建的 timeout_timer 计时被回调时（即入参的 run loop 运行时间到了，表示 run loop 要退出了）
        Boolean poll = sourceHandledThisLoop || (0ULL == timeout_context->termTSR);

        // ⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️⬇️
        // 如果当前是主线程并且 dispatchPort 不为空且 didDispatchPortLastTime 为 false
        //（ didDispatchPortLastTime 是在 do while 外声明的局部变量，初值为 true）
        if (MACH_PORT_NULL != dispatchPort && !didDispatchPortLastTime)
        {
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
            // macOS 下执行
            msg = (mach_msg_header_t *)msg_buffer;
            // 如果有 source1 被 signaled，则不休眠，直接跳到 handle_msg 去处理 source1
            // （主队列由任务要执行）
            if (__CFRunLoopServiceMachPort(dispatchPort, &msg, sizeof(msg_buffer), &livePort, 0, &voucherState, NULL))
            {
                goto handle_msg;
            }
#elif DEPLOYMENT_TARGET_WINDOWS
            if (__CFRunLoopWaitForMultipleObjects(NULL, &dispatchPort, 0, 0, &livePort, NULL))
            {
                goto handle_msg;
            }
#endif
        }
        // ⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️⬆️

        // didDispatchPortLastTime 置为 false
        didDispatchPortLastTime = false;

        // 若需要 poll 为假，则需要上面 sourceHandledThisLoop 为假即 rlm 的 _sources0 中没有 source 需要执行且 0ULL == timeout_context->termTSR，
        // 则调用 __CFRunLoopDoObservers 函数回调 rl 切换到 kCFRunLoopBeforeWaiting，即 rl 即将进入休眠状态。

        // 那么这里可以得出一个结论，当 run loop mode 中 source0 为空，
        // 或者 run loop 前一轮循环中 source0 中的 CFRunLoopSourceRef 的 perform 函数都已经执行完并被标记为 UnsetSignaled，
        // 并且入参的 seconds 小于等于 0 或者达到了入参 seconds 的运行时间，则 run loop 可以进入休眠
        if (!poll && (rlm->_observerMask & kCFRunLoopBeforeWaiting))
            __CFRunLoopDoObservers(rl, rlm, kCFRunLoopBeforeWaiting);

        // 设置 __CFBitfieldSetValue(((CFRuntimeBase *)rl)->_cfinfo[CF_INFO_BITS], 1, 1, 1)，
        // 标记 rl 进入休眠状态
        __CFRunLoopSetSleeping(rl);
        // do not do any user callouts after this point (after notifying of sleeping)
        // 在此之后（通知睡眠之后）不进行任何用户标注

        // Must push the local-to-this-activation ports in on every loop
        // iteration, as this mode could be run re-entrantly and we don't
        // want these ports to get serviced.
        // 必须在每次循环迭代中都将 local-to-this-activation 端口推入，因为此模式可以重新进入运行，我们不希望为这些端口提供服务。

        // 把 dispatchPort 插入到 rlm 的 _portSet 中（waitSet）
        __CFPortSetInsert(dispatchPort, waitSet);

        // CFRunLoopMode 解锁
        __CFRunLoopModeUnlock(rlm);
        // CFRunLoop 解锁
        __CFRunLoopUnlock(rl);

        // sleepStart 用于记录睡眠开始的时间，poll 为 false 时预示着进入休眠状态
        CFAbsoluteTime sleepStart = poll ? 0.0 : CFAbsoluteTimeGetCurrent();

#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
// 在 macOS 下
#if USE_DISPATCH_SOURCE_FOR_TIMERS
        // 如果 rlm 使用 dispatch_source 构建的计时器

        // 这个内层的 do while 循环主要是用于 "保持" run looop 的睡眠状态的，直到需要被唤醒了才会跳出这个 do while 循环。
        // 只有在下面的事件发生时才会进行唤醒：
        // 1. 基于端口的输入源（port-based input source）（source1）的事件到达。
        // 2. CFRunLoopMode 中的 timers 触发。（CFRunLoopMode 可添加多个 timer，它们共用一个 _timerPort 唤醒 run loop，
        //    并且会计算所有 timer 中最近的下次要触发的 timer 的时间）
        // 3. 为 run loop 设置的超时时间过期。
        // 4. run loop 被显式唤醒。（被其他什么调用者手动唤醒）
        do
        {
            if (kCFUseCollectableAllocator)
            {
                // objc_clear_stack(0);
                // <rdar://problem/16393959>
                // 把以 msg_buffer 为起点长度为 sizeof(msg_buffer) 的内存置为 0
                memset(msg_buffer, 0, sizeof(msg_buffer));
            }
            // 强转为 mach_msg_header_t 指针
            // uint8_t msg_buffer[3 * 1024];
            // mach_msg_header_t *msg = NULL;
            msg = (mach_msg_header_t *)msg_buffer;

            // MachPort
            // 端口消息（mach_msg），（正式进入休眠）
            // 等待 waitSet 中的端口发送消息，等待接收消息唤醒当前 run loop
            // run loop 的核心就是一个 mach_msg，run loop 调用这个函数去接收消息，如果没有别人发送 port 消息过来，内核会将线程置于等待状态
            __CFRunLoopServiceMachPort(waitSet,
                                       &msg,
                                       sizeof(msg_buffer),
                                       &livePort,
                                       poll ? 0 : TIMEOUT_INFINITY,
                                       &voucherState,
                                       &voucherCopy);

            // modeQueuePort = _dispatch_runloop_root_queue_get_port_4CF(rlm->_queue) 来自于 rlm 的 _queue 队列端口
            // 基于 port 的 source 事件 或 调用者唤醒
            if (modeQueuePort != MACH_PORT_NULL && livePort == modeQueuePort)
            {
                // Drain the internal queue. If one of the callout blocks sets the timerFired flag, break out and service the timer.
                // 清空内部队列。如果其中一个标注块设置了 timerFired 标志，请中断并为计时器提供服务。

                // 如果一直能取到 rlm 的 _queue 的端口则一直 while 循环
                while (_dispatch_runloop_root_queue_perform_4CF(rlm->_queue))
                    ;

                // _timerFired 首先赋值为 false，然后在 timer 的回调函数执行的时候会赋值为 true
                // rlm->_timerFired = false;
                // 当 _timerSource（计时器）回调时会执行这个 block，block 内部是把 _timerFired 修改为 true
                // __block Boolean *timerFiredPointer = &(rlm->_timerFired);
                // dispatch_source_set_event_handler(rlm->_timerSource, ^{
                //     *timerFiredPointer = true;
                // });
                // rlm 的 _timerSource 是启动时间是 DISPATCH_TIME_FOREVER，间隔是 DISPATCH_TIME_FOREVER 的计时器
                // _dispatch_source_set_runloop_timer_4CF(rlm->_timerSource, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 321);

                //  timer 时间到 或 run loop 超时
                if (rlm->_timerFired)
                {
                    // Leave livePort as the queue port, and service timers below
                    // 将 livePort 保留为队列端口，并在下面保留服务计时器。

                    // rlm 的 _timerSource 计时器回调后 run loop 会结束休眠
                    rlm->_timerFired = false;
                    // 离开内循环。
                    break;
                }
                else
                {
                    if (msg && msg != (mach_msg_header_t *)msg_buffer)
                        free(msg);
                }
            }
            else
            {
                // Go ahead and leave the inner loop.
                // 继续并离开内循环。
                break;
            }
        } while (1);
#else
        if (kCFUseCollectableAllocator)
        {
            // objc_clear_stack(0);
            // <rdar://problem/16393959>
            memset(msg_buffer, 0, sizeof(msg_buffer));
        }
        msg = (mach_msg_header_t *)msg_buffer;
        // 等待 waitSet 中的端口发送消息，等待接收消息
        __CFRunLoopServiceMachPort(waitSet, &msg, sizeof(msg_buffer), &livePort, poll ? 0 : TIMEOUT_INFINITY, &voucherState, &voucherCopy);
#endif

#elif DEPLOYMENT_TARGET_WINDOWS
        // Here, use the app-supplied message queue mask. They will set this if they are interested in having this run loop receive windows messages.
        __CFRunLoopWaitForMultipleObjects(waitSet, NULL, poll ? 0 : TIMEOUT_INFINITY, rlm->_msgQMask, &livePort, &windowsMessageReceived);
#endif

        __CFRunLoopLock(rl);
        __CFRunLoopModeLock(rlm);

        // 此时 run loop 要被唤醒了...

        // 统计 rl 的休眠时间，CFAbsoluteTimeGetCurrent() 当前时间减去 sleepStart 休眠开始时间
        rl->_sleepTime += (poll ? 0.0 : (CFAbsoluteTimeGetCurrent() - sleepStart));

        // Must remove the local-to-this-activation ports in on every loop
        // iteration, as this mode could be run re-entrantly and we don't
        // want these ports to get serviced. Also, we don't want them left
        // in there if this function returns.
        // 必须在每次循环迭代中都删除本地激活端口，因为此模式可以重新进入，并且我们不希望为这些端口提供服务。
        // 另外，如果此函数返回，我们不希望它们留在那里。

        // 从 waitSet 中移除 dispatchPort
        __CFPortSetRemove(dispatchPort, waitSet);

        // 设置 rl "忽略唤醒"，意指当前的 run loop 已经是唤醒状态了，此时再来唤醒的话直接 return
        // rl->_perRunData->ignoreWakeUps = 0x57414B45; // 'WAKE'
        __CFRunLoopSetIgnoreWakeUps(rl);

        // user callouts now OK again
        // __CFBitfieldSetValue(((CFRuntimeBase *)rl)->_cfinfo[CF_INFO_BITS], 1, 1, 0);
        // 标记 rl 为非休眠状态
        __CFRunLoopUnsetSleeping(rl);

        // 调用 __CFRunLoopDoObservers 函数，回调 rl 切换到 kCFRunLoopAfterWaiting 状态了
        // !poll 条件用于判断上面有进入休眠状态，通知 run loop observer 休眠要结束了
        if (!poll && (rlm->_observerMask & kCFRunLoopAfterWaiting))
            __CFRunLoopDoObservers(rl, rlm, kCFRunLoopAfterWaiting);

    handle_msg:;
        __CFRunLoopSetIgnoreWakeUps(rl);

// 一大段 windows 平台下的代码，可忽略
#if DEPLOYMENT_TARGET_WINDOWS
        if (windowsMessageReceived)
        {
            // These Win32 APIs cause a callout, so make sure we're unlocked first and relocked after
            __CFRunLoopModeUnlock(rlm);
            __CFRunLoopUnlock(rl);

            if (rlm->_msgPump)
            {
                rlm->_msgPump();
            }
            else
            {
                MSG msg;
                if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE | PM_NOYIELD))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }

            __CFRunLoopLock(rl);
            __CFRunLoopModeLock(rlm);
            sourceHandledThisLoop = true;

            // To prevent starvation of sources other than the message queue, we check again to see if any other sources need to be serviced
            // Use 0 for the mask so windows messages are ignored this time. Also use 0 for the timeout, because we're just checking to see if the things are signalled right now -- we will wait on them again later.
            // NOTE: Ignore the dispatch source (it's not in the wait set anymore) and also don't run the observers here since we are polling.
            __CFRunLoopSetSleeping(rl);
            __CFRunLoopModeUnlock(rlm);
            __CFRunLoopUnlock(rl);

            __CFRunLoopWaitForMultipleObjects(waitSet, NULL, 0, 0, &livePort, NULL);

            __CFRunLoopLock(rl);
            __CFRunLoopModeLock(rlm);
            __CFRunLoopUnsetSleeping(rl);
            // If we have a new live port then it will be handled below as normal
        }

#endif
        // 根据唤醒 run loop 的 livePort 值，来进行对应逻辑处理
        if (MACH_PORT_NULL == livePort)
        { // 如果 livePort 为 MACH_PORT_NULL，可能是 run loop 休眠超时，啥都不做
          // #define CFRUNLOOP_WAKEUP_FOR_NOTHING() do { } while (0)
            CFRUNLOOP_WAKEUP_FOR_NOTHING();
            // handle nothing
        }
        else if (livePort == rl->_wakeUpPort)
        { // 如果 livePort 是 rl->_wakeUpPort，是指被其他线程或进程唤醒，啥都不做
            // #define CFRUNLOOP_WAKEUP_FOR_WAKEUP() do { } while (0)
            CFRUNLOOP_WAKEUP_FOR_WAKEUP();
            // do nothing on Mac OS
#if DEPLOYMENT_TARGET_WINDOWS
            // Always reset the wake up port, or risk spinning forever
            ResetEvent(rl->_wakeUpPort);
#endif
        }
        // 如果计时器是使用 dispatch_source 实现的
#if USE_DISPATCH_SOURCE_FOR_TIMERS
        else if (modeQueuePort != MACH_PORT_NULL && livePort == modeQueuePort)
        {
            // 如果 rlm 的 queue 的 modeQueuePort 不为 NULL，且此时 livePort 等于 modeQueuePort，
            // 则表示此时需要处理 timer 回调。（这里表示是 timer 回调时间到了唤醒 run loop）

            // #define CFRUNLOOP_WAKEUP_FOR_TIMER() do { } while (0)
            CFRUNLOOP_WAKEUP_FOR_TIMER();
            // 遍历执行 rlm 的 _timers 集合中到达触发时间的 timer 的回调函数并更新其 `_fireTSR` 和 `_nextFireDate`
            if (!__CFRunLoopDoTimers(rl, rlm, mach_absolute_time()))
            {
                // Re-arm the next timer, because we apparently fired early

                // 目前我们收集到四个：
                // __CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__ // run loop 的状态发生变化前执行 run loop observer 的回调函数
                // __CFRUNLOOP_IS_CALLING_OUT_TO_A_BLOCK__ // 执行 run loop 的 block 链表中的 block（block 执行完以后会被释放并移除）
                // __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE0_PERFORM_FUNCTION__ // 执行 run loop mode 的 _source0 中的 CFRunLoopSourceRef 的 perfom 函数（以其 info 为参数）
                // __CFRUNLOOP_IS_CALLING_OUT_TO_A_TIMER_CALLBACK_FUNCTION__ // 执行 run loop mode 的 _timers 中的 CFRunLoopTimerRef 的 _callout 函数（以其 _context.info 为参数）

                // 重新布防下一个计时器（计算计时器中下次最近的触发时间，时间到时唤醒 run loop）
                __CFArmNextTimerInMode(rlm, rl);
            }
        }
#endif
#if USE_MK_TIMER_TOO
        else if (rlm->_timerPort != MACH_PORT_NULL && livePort == rlm->_timerPort)
        { // 如果计时器是使用 MK 实现的

            // #define   CFRUNLOOP_WAKEUP_FOR_TIMER() do { } while (0)
            // （这里表示是 timer 回调时间到了唤醒 run loop）
            CFRUNLOOP_WAKEUP_FOR_TIMER();
            // On Windows, we have observed an issue where the timer port is set before the time which we requested it to be set. 
            // For example, we set the fire time to be TSR 167646765860, but it is actually observed firing at TSR 167646764145, 
            // which is 1715 ticks early. The result is that, when __CFRunLoopDoTimers checks to see if any of the run loop timers should be firing, 
            // it appears to be 'too early' for the next timer, and no timers are handled.
            // 在 Windows 上，我们发现了一个问题，即在我们要求设置定时器端口之前设置了定时器端口。
            // 例如，我们将开火时间设置为 TSR 167646765860，但实际上可以观察到以 TSR 167646764145 开火，这是提早 1715 滴答。
            // 结果是，当 __CFRunLoopDoTimers 检查是否应触发任何运行循环计时器时，下一个计时器似乎为时过早，并且不处理任何计时器。

            // In this case, the timer port has been automatically reset (since it was returned from MsgWaitForMultipleObjectsEx), and if we do not re-arm it, then no timers will ever be serviced again unless something adjusts the timer list (e.g. adding or removing timers). The fix for the issue is to reset the timer here if CFRunLoopDoTimers did not handle a timer itself. 9308754
            // 在这种情况下，定时器端口已自动重置（因为它是从 MsgWaitForMultipleObjectsEx 返回的），如果我们不重新武装它，
            // 那么除非有什么东西调整了定时器列表（例如添加或 删除计时器）。 
            // 如果 CFRunLoopDoTimers 本身没有处理计时器，则该问题的解决方法是在此处重置计时器。 9308754

            // run loop mode 中使用 MK_TIMER 构建计时器时。触发到达执行时间的 run loop mode 中的 timer 回调。

            // 遍历执行 rlm 的 _timers 集合中到达触发时间的 timer 的回调函数并更新其 `_fireTSR` 和 `_nextFireDate`
            if (!__CFRunLoopDoTimers(rl, rlm, mach_absolute_time()))
            {
                // Re-arm the next timer
                // 重新布防下一个计时器

                __CFArmNextTimerInMode(rlm, rl);
            }
        }
#endif
        else if (livePort == dispatchPort)
        { // dispatchPort: 处理分发到 main queue 上的事件（这里表示是 dispatch 触发 run loop 唤醒）

            // #define   CFRUNLOOP_WAKEUP_FOR_DISPATCH() do { } while (0)
            CFRUNLOOP_WAKEUP_FOR_DISPATCH();
            // CFRunLoopMode 解锁
            __CFRunLoopModeUnlock(rlm);
            // CFRunLoop 解锁
            __CFRunLoopUnlock(rl);
            // 设置 TSD 中的 __CFTSDKeyIsInGCDMainQ 为 6（与下面的 0 成对，大概理解为构成锁）
            _CFSetTSD(__CFTSDKeyIsInGCDMainQ, (void *)6, NULL);
#if DEPLOYMENT_TARGET_WINDOWS
            void *msg = 0;
#endif
            // 目前我们收集到五个：
            // __CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__
            // __CFRUNLOOP_IS_CALLING_OUT_TO_A_BLOCK__
            // __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE0_PERFORM_FUNCTION__
            // __CFRUNLOOP_IS_CALLING_OUT_TO_A_TIMER_CALLBACK_FUNCTION__
            // __CFRUNLOOP_IS_SERVICING_THE_MAIN_DISPATCH_QUEUE__

            // 主队列回调事件
            __CFRUNLOOP_IS_SERVICING_THE_MAIN_DISPATCH_QUEUE__(msg);
            // 设置 TSD 中的 __CFTSDKeyIsInGCDMainQ 为 0
            _CFSetTSD(__CFTSDKeyIsInGCDMainQ, (void *)0, NULL);
            // CFRunLoop CFRunLoopMode 加锁
            __CFRunLoopLock(rl);
            __CFRunLoopModeLock(rlm);
            sourceHandledThisLoop = true;
            didDispatchPortLastTime = true;
        }
        else
        {
            // 其余的，肯定是各种 source1 事件

            // #define   CFRUNLOOP_WAKEUP_FOR_SOURCE() do { } while (0)
            // （这里表示是 source1 触发 run loop 唤醒）
            CFRUNLOOP_WAKEUP_FOR_SOURCE();

            // If we received a voucher from this mach_msg, then put a copy of the new voucher into TSD.
            // CFMachPortBoost will look in the TSD for the voucher.
            // By using the value in the TSD we tie the CFMachPortBoost to this received mach_msg explicitly
            // without a chance for anything in between the two pieces of code to set the voucher again.
            // 如果我们收到了来自此 mach_msg 的凭证，则将新凭证的副本放入 TSD。
            // CFMachPortBoost 将在 TSD 中查找该凭证。通过使用 TSD 中的值，我们将 CFMachPortBoost 明确地绑定到此接收到的 mach_msg 上，
            // 而在这两段代码之间没有任何机会再次设置凭单。

            // TSD 释放时，voucherCopy 调用 os_release 释放
             voucher_t previousVoucher = _CFSetTSD(__CFTSDKeyMachMessageHasVoucher, (void *)voucherCopy, os_release);

             // Despite the name, this works for windows handles as well
             // 从 rlm 的 _portToV1SourceMap 中，根据 livePort 找到其对应的 CFRunLoopSourceRef
             CFRunLoopSourceRef rls = __CFRunLoopModeFindSourceForMachPort(rl, rlm, livePort);
             if (rls)
             {
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
                mach_msg_header_t *reply = NULL;
                // 执行 source1 回调 __CFRUNLOOP_IS_CALLING_OUT_TO_A_SOURCE1_PERFORM_FUNCTION__
                sourceHandledThisLoop = __CFRunLoopDoSource1(rl, rlm, rls, msg, msg->msgh_size, &reply) || sourceHandledThisLoop;
                if (NULL != reply)
                {
                    (void)mach_msg(reply, MACH_SEND_MSG, reply->msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
                    CFAllocatorDeallocate(kCFAllocatorSystemDefault, reply);
                }
#elif DEPLOYMENT_TARGET_WINDOWS
                sourceHandledThisLoop = __CFRunLoopDoSource1(rl, rlm, rls) || sourceHandledThisLoop;
#endif
            }

            // Restore the previous voucher 恢复以前的凭证
            _CFSetTSD(__CFTSDKeyMachMessageHasVoucher, previousVoucher, os_release);
        }
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
        // 释放 msg 的内存空间
        if (msg && msg != (mach_msg_header_t *)msg_buffer)
            free(msg);
#endif
        // 执行 rl 的 block 链表中的 block
        __CFRunLoopDoBlocks(rl, rlm);

        // 根据当前 run loop 的状态来判断是否需要走下一个 loop。
        if (sourceHandledThisLoop && stopAfterHandle)
        {
            // stopAfterHandle 为真表示仅处理一个 sourc0，标记为 kCFRunLoopRunHandledSource。退出本次 run loop 循环。
            retVal = kCFRunLoopRunHandledSource;
        }
        else if (timeout_context->termTSR < mach_absolute_time())
        {
            // run loop 运行超时。退出本次 run loop 循环。
            retVal = kCFRunLoopRunTimedOut;
        }
        else if (__CFRunLoopIsStopped(rl))
        {
            // 外部停止。退出本次 run loop 循环。
            __CFRunLoopUnsetStopped(rl);
            retVal = kCFRunLoopRunStopped;
        }
        else if (rlm->_stopped)
        {
            // rlm 停止（外部强制停止）。退出本次 run loop 循环。
            rlm->_stopped = false;
            retVal = kCFRunLoopRunStopped;
        }
        else if (__CFRunLoopModeIsEmpty(rl, rlm, previousMode))
        {
            // rlm 的 sources0/sources1/timers/block 为空。退出本次 run loop 循环。
            retVal = kCFRunLoopRunFinished;
        }

#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
        voucher_mach_msg_revert(voucherState);
        os_release(voucherCopy);
#endif

    } while (0 == retVal); // 外层的 do while 循环结束的条件是 retVal 不等于 0 时

    // timeout_timer 是记录 run loop 运行时间的计时器，run loop 退出时要对 timeout_timer 进行释放
    if (timeout_timer)
    {
        // 取消计时器，会在取消的回调函数 __CFRunLoopTimeoutCancel 里面做清理工作
        dispatch_source_cancel(timeout_timer);
        // 释放 timeout_timer
        dispatch_release(timeout_timer);
    }
    else
    {
        // 释放 timeout_context，
        // 对应前面的 timeout_context = (struct __timeout_context *)malloc(sizeof(*timeout_context))，malloc 的申请空间。
        free(timeout_context);
    }

    return retVal;
}

/*
CFRunLoopRunSpecific 函数内部会调用 __CFRunLoopRun 函数，可以把 __CFRunLoopRun 的调用看作一个分界线。

调用前：
首先判断 rl 是否被标记为正在销毁，如果是的话则直接返回 kCFRunLoopRunFinished，否则继续往下执行，
根据 modeName 从 rl 的 _modes 中找到其对应的 CFRunLoopModeRef，如果未找到或者 CFRunLoopModeRef 的
sources0/sources1/timers/block 为空，则也是直接返回  kCFRunLoopRunFinished。
然后是修改 rl 的 _perRunData 和 _currentMode 同时还会记录之前的旧值，此时一切准备就绪，
在调用之前会根据 rl 的 _currentMode 的 _observerMask 判断是否需要回调 run loop observer
观察者来告诉它们 run loop 要进入 kCFRunLoopEntry 状态了，然后调用 __CFRunLoopRun 函数正式启动 run loop。

 __CFRunLoopRun 函数返回后则是，首先根据 rl 的 _currentMode 的 _observerMask 判断是否需要回调
run loop observer 观察者来告诉它们 run loop 要进入 kCFRunLoopExit 状态了。
然后是把 run loop 对象恢复到之前的 _perRunData 和 _currentMode（处理 run loop 的嵌套）。

*/
SInt32 CFRunLoopRunSpecific(CFRunLoopRef rl,
                            CFStringRef modeName,
                            CFTimeInterval seconds,
                            Boolean returnAfterSourceHandled)
{ /* DOES CALLOUT */
    CHECK_FOR_FORK();
    // 从 rl 的 _cfinfo 字段中取 rl 是否正在销毁的标记值，如果是的话，则直接返回 kCFRunLoopRunFinished
    if (__CFRunLoopIsDeallocating(rl))
        return kCFRunLoopRunFinished;
    // CFRunLoop 加锁
    __CFRunLoopLock(rl);

    // 调用 __CFRunLoopFindMode 函数从 rl 的 _modes 中找到名字是 modeName 的 run loop mode，
    // 如果找不到的话第三个参数传的是 false 则不进行新建 run loop mode，则直接返回 NULL。
    //（CFRunLoopMode 加锁）
    CFRunLoopModeRef currentMode = __CFRunLoopFindMode(rl, modeName, false);

    // 如果 currentMode 为 NULL 或者 currentMode 里面是空的不包含 sources0/sources1/timers/block 则 return
    if (NULL == currentMode || __CFRunLoopModeIsEmpty(rl, currentMode, rl->_currentMode))
    {
        Boolean did = false;
        // 如果 currentMode 存在，则进行 CFRunLoopMode 解锁，
        // 对应了上面 __CFRunLoopFindMode(rl, modeName, false) 调用内部的 CFRunLoopMode 加锁
        if (currentMode)
            __CFRunLoopModeUnlock(currentMode);
        // CFRunLoop 解锁
        __CFRunLoopUnlock(rl);
        // 返回 kCFRunLoopRunFinished
        return did ? kCFRunLoopRunHandledSource : kCFRunLoopRunFinished;
    }
    // __CFRunLoopPushPerRunData 函数内部是修改 rl 的 _perRunData 字段的各成员变量的值，并返回之前的 _perRunData，
    //（函数内部修改 _perRunData 的值其实是在标记 run loop 不同状态）
    //（这里的 previousPerRun 是用于下面的 __CFRunLoopRun 函数调用返回后，当前的 run loop 对象要回到之前的 _perRunData）。
    volatile _per_run_data *previousPerRun = __CFRunLoopPushPerRunData(rl);

    // previousMode 记录 rl 当前的 run loop mode，相比入参传入的 modeName 取得的 run loop mode 而言，它是之前的 run loop mode，
    // 这个 previousMode 主要用于下面的那行 __CFRunLoopRun 函数调用返回后，当前的 run loop 对象要回到之前的 run loop mode。
    //（同上面的 previousPerRun 数据，也要把当前的 run loop 对象回到之前的 _perRunData 数据的状态）
    CFRunLoopModeRef previousMode = rl->_currentMode;

    // 更新 rl 的 _currentMode 为入参 modeName 对应的 run loop mode
    rl->_currentMode = currentMode;

    // 临时变量 result，用于当函数返回时记录 run loop 不同的退出原因
    int32_t result = kCFRunLoopRunFinished;

    // 判断如果 currentMode 的 _observerMask 字段中包含 kCFRunLoopEntry 的值（_observerMask 内记录了需要观察 run loop 哪些状态变化），
    // 则告诉 currentMode 的 run loop observer 发生了一个 run loop 即将进入循环的状态变化。
    if (currentMode->_observerMask & kCFRunLoopEntry)
        __CFRunLoopDoObservers(rl, currentMode, kCFRunLoopEntry);

    //!!!!!!!!!!!!!
    // 启动 run loop
    result = __CFRunLoopRun(rl, currentMode, seconds, returnAfterSourceHandled, previousMode);

    // ⬆️⬆️⬆️ __CFRunLoopRun 函数好像也是不会返回的，当它返回时就代表当前的 run loop 要退出了。

    // 同上的 kCFRunLoopEntry 进入循环的回调，这里则是退出 run loop 的回调。
    // 如果 currentMode 的 _observerMask 中包含 kCFRunLoopExit 的值，
    // 即 run loop observer 需要观察 run loop 的 kCFRunLoopExit 退出状态切换
    if (currentMode->_observerMask & kCFRunLoopExit)
        __CFRunLoopDoObservers(rl, currentMode, kCFRunLoopExit);

    // CFRunLoopMode 解锁
    __CFRunLoopModeUnlock(currentMode);
    // 销毁 rl 当前的 _perRunData，并把 previousPerRun 重新赋值给 rl 的 _perRunData
    __CFRunLoopPopPerRunData(rl, previousPerRun);
    // 回到之前的 _currentMode
    rl->_currentMode = previousMode;
    // CFRunLoop 解锁
    __CFRunLoopUnlock(rl);
    // 返回 result 结果
    return result;
}

/// CFRunLoopRun 函数是内部是一个 do while 循环，内部调用了 CFRunLoopRunSpecific 函数
void CFRunLoopRun(void)
{ /* DOES CALLOUT */
    int32_t result;
    do
    {
        // 调用 CFRunLoopRunSpecific 函数，以 kCFRunLoopDefaultMode 启动当前线程的 run loop，运行时间传入的是 10^10 秒（2777777 个小时），
        // returnAfterSourceHandled 参数传入的是 false，指示 run loop 是在处理一个源之后不退出并持续处理事件。
        result = CFRunLoopRunSpecific(CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, 1.0e10, false);
        CHECK_FOR_FORK();
    } while (kCFRunLoopRunStopped != result && kCFRunLoopRunFinished != result);
}

SInt32 CFRunLoopRunInMode(CFStringRef modeName, CFTimeInterval seconds, Boolean returnAfterSourceHandled)
{ /* DOES CALLOUT */
    CHECK_FOR_FORK();
    return CFRunLoopRunSpecific(CFRunLoopGetCurrent(), modeName, seconds, returnAfterSourceHandled);
}

CFAbsoluteTime CFRunLoopGetNextTimerFireDate(CFRunLoopRef rl, CFStringRef modeName)
{
    CHECK_FOR_FORK();
    __CFRunLoopLock(rl);
    CFRunLoopModeRef rlm = __CFRunLoopFindMode(rl, modeName, false);
    CFAbsoluteTime at = 0.0;
    CFRunLoopTimerRef nextTimer = (rlm && rlm->_timers && 0 < CFArrayGetCount(rlm->_timers)) ? (CFRunLoopTimerRef)CFArrayGetValueAtIndex(rlm->_timers, 0) : NULL;
    if (nextTimer)
    {
        at = CFRunLoopTimerGetNextFireDate(nextTimer);
    }
    if (rlm)
        __CFRunLoopModeUnlock(rlm);
    __CFRunLoopUnlock(rl);
    return at;
}

Boolean CFRunLoopIsWaiting(CFRunLoopRef rl)
{
    CHECK_FOR_FORK();
    return __CFRunLoopIsSleeping(rl);
}

void CFRunLoopWakeUp(CFRunLoopRef rl)
{
    CHECK_FOR_FORK();
    // This lock is crucial to ignorable wakeups, do not remove it.
    __CFRunLoopLock(rl);
    if (__CFRunLoopIsIgnoringWakeUps(rl))
    {
        __CFRunLoopUnlock(rl);
        return;
    }
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
    kern_return_t ret;
    /* We unconditionally try to send the message, since we don't want
     * to lose a wakeup, but the send may fail if there is already a
     * wakeup pending, since the queue length is 1. */
    ret = __CFSendTrivialMachMessage(rl->_wakeUpPort, 0, MACH_SEND_TIMEOUT, 0);
    if (ret != MACH_MSG_SUCCESS && ret != MACH_SEND_TIMED_OUT)
        CRASH("*** Unable to send message to wake up port. (%d) ***", ret);
#elif DEPLOYMENT_TARGET_WINDOWS
    SetEvent(rl->_wakeUpPort);
#endif
    __CFRunLoopUnlock(rl);
}

void CFRunLoopStop(CFRunLoopRef rl)
{
    Boolean doWake = false;
    CHECK_FOR_FORK();
    __CFRunLoopLock(rl);
    if (rl->_currentMode)
    {
        __CFRunLoopSetStopped(rl);
        doWake = true;
    }
    __CFRunLoopUnlock(rl);
    if (doWake)
    {
        CFRunLoopWakeUp(rl);
    }
}

CF_EXPORT void _CFRunLoopStopMode(CFRunLoopRef rl, CFStringRef modeName)
{
    CHECK_FOR_FORK();
    CFRunLoopModeRef rlm;
    __CFRunLoopLock(rl);
    rlm = __CFRunLoopFindMode(rl, modeName, true);
    if (NULL != rlm)
    {
        rlm->_stopped = true;
        __CFRunLoopModeUnlock(rlm);
    }
    __CFRunLoopUnlock(rl);
    CFRunLoopWakeUp(rl);
}

CF_EXPORT Boolean _CFRunLoopModeContainsMode(CFRunLoopRef rl, CFStringRef modeName, CFStringRef candidateContainedName)
{
    CHECK_FOR_FORK();
    return false;
}

void CFRunLoopPerformBlock(CFRunLoopRef rl, CFTypeRef mode, void (^block)(void))
{
    CHECK_FOR_FORK();
    if (CFStringGetTypeID() == CFGetTypeID(mode))
    {
        mode = CFStringCreateCopy(kCFAllocatorSystemDefault, (CFStringRef)mode);
        __CFRunLoopLock(rl);
        // ensure mode exists
        CFRunLoopModeRef currentMode = __CFRunLoopFindMode(rl, (CFStringRef)mode, true);
        if (currentMode)
            __CFRunLoopModeUnlock(currentMode);
        __CFRunLoopUnlock(rl);
    }
    else if (CFArrayGetTypeID() == CFGetTypeID(mode))
    {
        CFIndex cnt = CFArrayGetCount((CFArrayRef)mode);
        const void **values = (const void **)malloc(sizeof(const void *) * cnt);
        CFArrayGetValues((CFArrayRef)mode, CFRangeMake(0, cnt), values);
        mode = CFSetCreate(kCFAllocatorSystemDefault, values, cnt, &kCFTypeSetCallBacks);
        __CFRunLoopLock(rl);
        // ensure modes exist
        for (CFIndex idx = 0; idx < cnt; idx++)
        {
            CFRunLoopModeRef currentMode = __CFRunLoopFindMode(rl, (CFStringRef)values[idx], true);
            if (currentMode)
                __CFRunLoopModeUnlock(currentMode);
        }
        __CFRunLoopUnlock(rl);
        free(values);
    }
    else if (CFSetGetTypeID() == CFGetTypeID(mode))
    {
        CFIndex cnt = CFSetGetCount((CFSetRef)mode);
        const void **values = (const void **)malloc(sizeof(const void *) * cnt);
        CFSetGetValues((CFSetRef)mode, values);
        mode = CFSetCreate(kCFAllocatorSystemDefault, values, cnt, &kCFTypeSetCallBacks);
        __CFRunLoopLock(rl);
        // ensure modes exist
        for (CFIndex idx = 0; idx < cnt; idx++)
        {
            CFRunLoopModeRef currentMode = __CFRunLoopFindMode(rl, (CFStringRef)values[idx], true);
            if (currentMode)
                __CFRunLoopModeUnlock(currentMode);
        }
        __CFRunLoopUnlock(rl);
        free(values);
    }
    else
    {
        mode = NULL;
    }
    block = Block_copy(block);
    if (!mode || !block)
    {
        if (mode)
            CFRelease(mode);
        if (block)
            Block_release(block);
        return;
    }
    __CFRunLoopLock(rl);
    struct _block_item *new_item = (struct _block_item *)malloc(sizeof(struct _block_item));
    new_item->_next = NULL;
    new_item->_mode = mode;
    new_item->_block = block;
    if (!rl->_blocks_tail)
    {
        rl->_blocks_head = new_item;
    }
    else
    {
        rl->_blocks_tail->_next = new_item;
    }
    rl->_blocks_tail = new_item;
    __CFRunLoopUnlock(rl);
}

Boolean CFRunLoopContainsSource(CFRunLoopRef rl, CFRunLoopSourceRef rls, CFStringRef modeName)
{
    CHECK_FOR_FORK();
    CFRunLoopModeRef rlm;
    Boolean hasValue = false;
    __CFRunLoopLock(rl);
    if (modeName == kCFRunLoopCommonModes)
    {
        if (NULL != rl->_commonModeItems)
        {
            hasValue = CFSetContainsValue(rl->_commonModeItems, rls);
        }
    }
    else
    {
        rlm = __CFRunLoopFindMode(rl, modeName, false);
        if (NULL != rlm)
        {
            hasValue = (rlm->_sources0 ? CFSetContainsValue(rlm->_sources0, rls) : false) || (rlm->_sources1 ? CFSetContainsValue(rlm->_sources1, rls) : false);
            __CFRunLoopModeUnlock(rlm);
        }
    }
    __CFRunLoopUnlock(rl);
    return hasValue;
}

void CFRunLoopAddSource(CFRunLoopRef rl, CFRunLoopSourceRef rls, CFStringRef modeName)
{ /* DOES CALLOUT */
    CHECK_FOR_FORK();
    if (__CFRunLoopIsDeallocating(rl))
        return;
    if (!__CFIsValid(rls))
        return;
    Boolean doVer0Callout = false;
    __CFRunLoopLock(rl);
    if (modeName == kCFRunLoopCommonModes)
    {
        CFSetRef set = rl->_commonModes ? CFSetCreateCopy(kCFAllocatorSystemDefault, rl->_commonModes) : NULL;
        if (NULL == rl->_commonModeItems)
        {
            rl->_commonModeItems = CFSetCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeSetCallBacks);
        }
        CFSetAddValue(rl->_commonModeItems, rls);
        if (NULL != set)
        {
            CFTypeRef context[2] = {rl, rls};
            /* add new item to all common-modes */
            CFSetApplyFunction(set, (__CFRunLoopAddItemToCommonModes), (void *)context);
            CFRelease(set);
        }
    }
    else
    {
        CFRunLoopModeRef rlm = __CFRunLoopFindMode(rl, modeName, true);
        if (NULL != rlm && NULL == rlm->_sources0)
        {
            rlm->_sources0 = CFSetCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeSetCallBacks);
            rlm->_sources1 = CFSetCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeSetCallBacks);
            rlm->_portToV1SourceMap = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, NULL, NULL);
        }
        if (NULL != rlm && !CFSetContainsValue(rlm->_sources0, rls) && !CFSetContainsValue(rlm->_sources1, rls))
        {
            if (0 == rls->_context.version0.version)
            {
                CFSetAddValue(rlm->_sources0, rls);
            }
            else if (1 == rls->_context.version0.version)
            {
                CFSetAddValue(rlm->_sources1, rls);
                __CFPort src_port = rls->_context.version1.getPort(rls->_context.version1.info);
                if (CFPORT_NULL != src_port)
                {
                    CFDictionarySetValue(rlm->_portToV1SourceMap, (const void *)(uintptr_t)src_port, rls);
                    __CFPortSetInsert(src_port, rlm->_portSet);
                }
            }
            __CFRunLoopSourceLock(rls);
            if (NULL == rls->_runLoops)
            {
                rls->_runLoops = CFBagCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeBagCallBacks); // sources retain run loops!
            }
            CFBagAddValue(rls->_runLoops, rl);
            __CFRunLoopSourceUnlock(rls);
            if (0 == rls->_context.version0.version)
            {
                if (NULL != rls->_context.version0.schedule)
                {
                    doVer0Callout = true;
                }
            }
        }
        if (NULL != rlm)
        {
            __CFRunLoopModeUnlock(rlm);
        }
    }
    __CFRunLoopUnlock(rl);
    if (doVer0Callout)
    {
        // although it looses some protection for the source, we have no choice but
        // to do this after unlocking the run loop and mode locks, to avoid deadlocks
        // where the source wants to take a lock which is already held in another
        // thread which is itself waiting for a run loop/mode lock
        rls->_context.version0.schedule(rls->_context.version0.info, rl, modeName); /* CALLOUT */
    }
}

void CFRunLoopRemoveSource(CFRunLoopRef rl, CFRunLoopSourceRef rls, CFStringRef modeName)
{ /* DOES CALLOUT */
    CHECK_FOR_FORK();
    Boolean doVer0Callout = false, doRLSRelease = false;
    __CFRunLoopLock(rl);
    if (modeName == kCFRunLoopCommonModes)
    {
        if (NULL != rl->_commonModeItems && CFSetContainsValue(rl->_commonModeItems, rls))
        {
            CFSetRef set = rl->_commonModes ? CFSetCreateCopy(kCFAllocatorSystemDefault, rl->_commonModes) : NULL;
            CFSetRemoveValue(rl->_commonModeItems, rls);
            if (NULL != set)
            {
                CFTypeRef context[2] = {rl, rls};
                /* remove new item from all common-modes */
                CFSetApplyFunction(set, (__CFRunLoopRemoveItemFromCommonModes), (void *)context);
                CFRelease(set);
            }
        }
        else
        {
        }
    }
    else
    {
        CFRunLoopModeRef rlm = __CFRunLoopFindMode(rl, modeName, false);
        if (NULL != rlm && ((NULL != rlm->_sources0 && CFSetContainsValue(rlm->_sources0, rls)) || (NULL != rlm->_sources1 && CFSetContainsValue(rlm->_sources1, rls))))
        {
            CFRetain(rls);
            if (1 == rls->_context.version0.version)
            {
                __CFPort src_port = rls->_context.version1.getPort(rls->_context.version1.info);
                if (CFPORT_NULL != src_port)
                {
                    CFDictionaryRemoveValue(rlm->_portToV1SourceMap, (const void *)(uintptr_t)src_port);
                    __CFPortSetRemove(src_port, rlm->_portSet);
                }
            }
            CFSetRemoveValue(rlm->_sources0, rls);
            CFSetRemoveValue(rlm->_sources1, rls);
            __CFRunLoopSourceLock(rls);
            if (NULL != rls->_runLoops)
            {
                CFBagRemoveValue(rls->_runLoops, rl);
            }
            __CFRunLoopSourceUnlock(rls);
            if (0 == rls->_context.version0.version)
            {
                if (NULL != rls->_context.version0.cancel)
                {
                    doVer0Callout = true;
                }
            }
            doRLSRelease = true;
        }
        if (NULL != rlm)
        {
            __CFRunLoopModeUnlock(rlm);
        }
    }
    __CFRunLoopUnlock(rl);
    if (doVer0Callout)
    {
        // although it looses some protection for the source, we have no choice but
        // to do this after unlocking the run loop and mode locks, to avoid deadlocks
        // where the source wants to take a lock which is already held in another
        // thread which is itself waiting for a run loop/mode lock
        rls->_context.version0.cancel(rls->_context.version0.info, rl, modeName); /* CALLOUT */
    }
    if (doRLSRelease)
        CFRelease(rls);
}

static void __CFRunLoopRemoveSourcesFromCommonMode(const void *value, void *ctx)
{
    CFStringRef modeName = (CFStringRef)value;
    CFRunLoopRef rl = (CFRunLoopRef)ctx;
    __CFRunLoopRemoveAllSources(rl, modeName);
}

static void __CFRunLoopRemoveSourceFromMode(const void *value, void *ctx)
{
    CFRunLoopSourceRef rls = (CFRunLoopSourceRef)value;
    CFRunLoopRef rl = (CFRunLoopRef)(((CFTypeRef *)ctx)[0]);
    CFStringRef modeName = (CFStringRef)(((CFTypeRef *)ctx)[1]);
    CFRunLoopRemoveSource(rl, rls, modeName);
}

static void __CFRunLoopRemoveAllSources(CFRunLoopRef rl, CFStringRef modeName)
{
    CHECK_FOR_FORK();
    CFRunLoopModeRef rlm;
    __CFRunLoopLock(rl);
    if (modeName == kCFRunLoopCommonModes)
    {
        if (NULL != rl->_commonModeItems)
        {
            CFSetRef set = rl->_commonModes ? CFSetCreateCopy(kCFAllocatorSystemDefault, rl->_commonModes) : NULL;
            if (NULL != set)
            {
                CFSetApplyFunction(set, (__CFRunLoopRemoveSourcesFromCommonMode), (void *)rl);
                CFRelease(set);
            }
        }
        else
        {
        }
    }
    else
    {
        rlm = __CFRunLoopFindMode(rl, modeName, false);
        if (NULL != rlm && NULL != rlm->_sources0)
        {
            CFSetRef set = CFSetCreateCopy(kCFAllocatorSystemDefault, rlm->_sources0);
            CFTypeRef context[2] = {rl, modeName};
            CFSetApplyFunction(set, (__CFRunLoopRemoveSourceFromMode), (void *)context);
            CFRelease(set);
        }
        if (NULL != rlm && NULL != rlm->_sources1)
        {
            CFSetRef set = CFSetCreateCopy(kCFAllocatorSystemDefault, rlm->_sources1);
            CFTypeRef context[2] = {rl, modeName};
            CFSetApplyFunction(set, (__CFRunLoopRemoveSourceFromMode), (void *)context);
            CFRelease(set);
        }
        if (NULL != rlm)
        {
            __CFRunLoopModeUnlock(rlm);
        }
    }
    __CFRunLoopUnlock(rl);
}

Boolean CFRunLoopContainsObserver(CFRunLoopRef rl, CFRunLoopObserverRef rlo, CFStringRef modeName)
{
    CHECK_FOR_FORK();
    CFRunLoopModeRef rlm;
    Boolean hasValue = false;
    __CFRunLoopLock(rl);
    if (modeName == kCFRunLoopCommonModes)
    {
        if (NULL != rl->_commonModeItems)
        {
            hasValue = CFSetContainsValue(rl->_commonModeItems, rlo);
        }
    }
    else
    {
        rlm = __CFRunLoopFindMode(rl, modeName, false);
        if (NULL != rlm && NULL != rlm->_observers)
        {
            hasValue = CFArrayContainsValue(rlm->_observers, CFRangeMake(0, CFArrayGetCount(rlm->_observers)), rlo);
        }
        if (NULL != rlm)
        {
            __CFRunLoopModeUnlock(rlm);
        }
    }
    __CFRunLoopUnlock(rl);
    return hasValue;
}

void CFRunLoopAddObserver(CFRunLoopRef rl, CFRunLoopObserverRef rlo, CFStringRef modeName)
{
    CHECK_FOR_FORK();
    CFRunLoopModeRef rlm;
    if (__CFRunLoopIsDeallocating(rl))
        return;
    if (!__CFIsValid(rlo) || (NULL != rlo->_runLoop && rlo->_runLoop != rl))
        return;
    __CFRunLoopLock(rl);
    if (modeName == kCFRunLoopCommonModes)
    {
        CFSetRef set = rl->_commonModes ? CFSetCreateCopy(kCFAllocatorSystemDefault, rl->_commonModes) : NULL;
        if (NULL == rl->_commonModeItems)
        {
            rl->_commonModeItems = CFSetCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeSetCallBacks);
        }
        CFSetAddValue(rl->_commonModeItems, rlo);
        if (NULL != set)
        {
            CFTypeRef context[2] = {rl, rlo};
            /* add new item to all common-modes */
            CFSetApplyFunction(set, (__CFRunLoopAddItemToCommonModes), (void *)context);
            CFRelease(set);
        }
    }
    else
    {
        rlm = __CFRunLoopFindMode(rl, modeName, true);
        if (NULL != rlm && NULL == rlm->_observers)
        {
            rlm->_observers = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
        }
        if (NULL != rlm && !CFArrayContainsValue(rlm->_observers, CFRangeMake(0, CFArrayGetCount(rlm->_observers)), rlo))
        {
            Boolean inserted = false;
            for (CFIndex idx = CFArrayGetCount(rlm->_observers); idx--;)
            {
                CFRunLoopObserverRef obs = (CFRunLoopObserverRef)CFArrayGetValueAtIndex(rlm->_observers, idx);
                if (obs->_order <= rlo->_order)
                {
                    CFArrayInsertValueAtIndex(rlm->_observers, idx + 1, rlo);
                    inserted = true;
                    break;
                }
            }
            if (!inserted)
            {
                CFArrayInsertValueAtIndex(rlm->_observers, 0, rlo);
            }
            rlm->_observerMask |= rlo->_activities;
            __CFRunLoopObserverSchedule(rlo, rl, rlm);
        }
        if (NULL != rlm)
        {
            __CFRunLoopModeUnlock(rlm);
        }
    }
    __CFRunLoopUnlock(rl);
}

void CFRunLoopRemoveObserver(CFRunLoopRef rl, CFRunLoopObserverRef rlo, CFStringRef modeName)
{
    CHECK_FOR_FORK();
    CFRunLoopModeRef rlm;
    __CFRunLoopLock(rl);
    if (modeName == kCFRunLoopCommonModes)
    {
        if (NULL != rl->_commonModeItems && CFSetContainsValue(rl->_commonModeItems, rlo))
        {
            CFSetRef set = rl->_commonModes ? CFSetCreateCopy(kCFAllocatorSystemDefault, rl->_commonModes) : NULL;
            CFSetRemoveValue(rl->_commonModeItems, rlo);
            if (NULL != set)
            {
                CFTypeRef context[2] = {rl, rlo};
                /* remove new item from all common-modes */
                CFSetApplyFunction(set, (__CFRunLoopRemoveItemFromCommonModes), (void *)context);
                CFRelease(set);
            }
        }
        else
        {
        }
    }
    else
    {
        rlm = __CFRunLoopFindMode(rl, modeName, false);
        if (NULL != rlm && NULL != rlm->_observers)
        {
            CFRetain(rlo);
            CFIndex idx = CFArrayGetFirstIndexOfValue(rlm->_observers, CFRangeMake(0, CFArrayGetCount(rlm->_observers)), rlo);
            if (kCFNotFound != idx)
            {
                CFArrayRemoveValueAtIndex(rlm->_observers, idx);
                __CFRunLoopObserverCancel(rlo, rl, rlm);
            }
            CFRelease(rlo);
        }
        if (NULL != rlm)
        {
            __CFRunLoopModeUnlock(rlm);
        }
    }
    __CFRunLoopUnlock(rl);
}

Boolean CFRunLoopContainsTimer(CFRunLoopRef rl, CFRunLoopTimerRef rlt, CFStringRef modeName)
{
    CHECK_FOR_FORK();
    if (NULL == rlt->_runLoop || rl != rlt->_runLoop)
        return false;
    Boolean hasValue = false;
    __CFRunLoopLock(rl);
    if (modeName == kCFRunLoopCommonModes)
    {
        if (NULL != rl->_commonModeItems)
        {
            hasValue = CFSetContainsValue(rl->_commonModeItems, rlt);
        }
    }
    else
    {
        CFRunLoopModeRef rlm = __CFRunLoopFindMode(rl, modeName, false);
        if (NULL != rlm)
        {
            if (NULL != rlm->_timers)
            {
                CFIndex idx = CFArrayGetFirstIndexOfValue(rlm->_timers, CFRangeMake(0, CFArrayGetCount(rlm->_timers)), rlt);
                hasValue = (kCFNotFound != idx);
            }
            __CFRunLoopModeUnlock(rlm);
        }
    }
    __CFRunLoopUnlock(rl);
    return hasValue;
}

/// runloop添加timer
/*
CFRunLoopAddTimer 将 CFRunLoopTimer 对象添加到 run loop mode。
modeName 是要将计时器添加到的 rl 的运行循环模式。
使用常数 kCFRunLoopCommonModes 将计时器添加到所有 common mode 监视的对象集中。
 尽管可以将一个运行循环计时器一次添加到一个运行循环中的多个运行循环模式中，但它一次只能在一个运行循环中注册。
如果 rl 已经在模式下包含计时器，则此功能不执行任何操作。

*/
void CFRunLoopAddTimer(CFRunLoopRef rl, CFRunLoopTimerRef rlt, CFStringRef modeName)
{
    // 用于检查给定的进程是否被 fork
    CHECK_FOR_FORK();
    // 如果 rl 被标记为正在进行释放，则直接返回。
    if (__CFRunLoopIsDeallocating(rl))
        return;
    // rlt 无效则 return，
    // 或者 rlt 的 _runLoop 不为 NULL 且 _runLoop 不等于 rl 的话直接 return
    if (!__CFIsValid(rlt) || (NULL != rlt->_runLoop && rlt->_runLoop != rl))
        return;
    // run loop 加锁
    __CFRunLoopLock(rl);

    // 如果 rlt 添加到 common mode 中
    if (modeName == kCFRunLoopCommonModes)
    {
        // 取得 rl 的 _commonModes 的副本
        CFSetRef set = rl->_commonModes ? CFSetCreateCopy(kCFAllocatorSystemDefault, rl->_commonModes) : NULL;
        if (NULL == rl->_commonModeItems)
        { //如果rl 的_commonModeItems 为空， 则创建一个空的set集合，并赋值给_commonModeItems
            rl->_commonModeItems = CFSetCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeSetCallBacks);
        }
        // 把 rlt 添加到 rl 的 _commonModeItems
        CFSetAddValue(rl->_commonModeItems, rlt);
        // 如果 rl 存在 common mode
        if (NULL != set)
        {
            CFTypeRef context[2] = {rl, rlt};
            /* add new item to all common-modes */
            /* CFSetApplyFunction：为set中的每个item都执行一次__CFRunLoopAddItemToCommonModes方法，
            参数是context
            _commonModes里存的是modeName
            调用 __CFRunLoopAddItemToCommonModes 函数把 rlt 添加到 rl 被标记为 common 的 mode 中
            */
            CFSetApplyFunction(set, (__CFRunLoopAddItemToCommonModes), (void *)context);
            // 释放上面的临时变量 set
            CFRelease(set);
        }
    }
    //  modeName 当前是 common mode 之外的普通 mode
    else
    {
        // 在 rl 中找到该 run loop mode，（并且进行 mode 的加锁）
        CFRunLoopModeRef rlm = __CFRunLoopFindMode(rl, modeName, true);
        // 如果找到了 rlm
        if (NULL != rlm)
        {
            // 如果 rlm 的 _timers 为 NULL 的话，则为其申请内存
            if (NULL == rlm->_timers)
            {
                CFArrayCallBacks cb = kCFTypeArrayCallBacks;
                cb.equal = NULL;
                // 为 _timers 申请内存
                rlm->_timers = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &cb);
            }
        }
        // 如果 rlm 不为 NULL，且 rlt 的 _rlModes 不包含 rlm
        if (NULL != rlm && !CFSetContainsValue(rlt->_rlModes, rlm->_name))
        {
            __CFRunLoopTimerLock(rlt);
            if (NULL == rlt->_runLoop)
            { // 如果 rlt->_runLoop 为 NULL，则为 rlt 的 _runLoop 赋值
                rlt->_runLoop = rl;
            }
            else if (rl != rlt->_runLoop)
            { // 如果 rlt 的 _runLoop 不为 rl，即此时 rlt 的 _runLoop 有值，
              // 则纷纷解锁后直接 return（即如果 timer 被添加过一个 run loop 了，就不能再被添加到别的 run loop 了。）
                __CFRunLoopTimerUnlock(rlt);
                __CFRunLoopModeUnlock(rlm);
                __CFRunLoopUnlock(rl);
                // 直接return
                return;
            }
            // 把 rlm 的 _name 添加到 rlt 的 _rlModes 中。
            //（也验证了 CFMutableSetRef _rlModes; timer 对应的 run loop modes，
            // 内部保存的也是 run loop mode 的名字，也验证了 timer 可以在多个 run loop mode 中使用）
            CFSetAddValue(rlt->_rlModes, rlm->_name);

            // timer 解锁
            __CFRunLoopTimerUnlock(rlt);
            // 加锁
            __CFRunLoopTimerFireTSRLock();
            // 把 rlt 添加到 rlm 的 _timers 中
            __CFRepositionTimerInMode(rlm, rlt, false);
            // 解锁
            __CFRunLoopTimerFireTSRUnlock();

            // CFSystemVersionLion = 7,        /* 10.7 */
            if (!_CFExecutableLinkedOnOrAfter(CFSystemVersionLion))
            {
                // Normally we don't do this on behalf of clients, but for
                // backwards compatibility due to the change in timer handling...
                // 通常我们不代表客户这样做，而是由于计时器处理方式的变化而向后兼容...
                if (rl != CFRunLoopGetCurrent())
                    CFRunLoopWakeUp(rl);
            }
        }
        // 这个对应的加锁是在 __CFRunLoopFindMode(rl, modeName, true) 函数内找到 rlm 并返回之前加的。
        if (NULL != rlm)
        {
            __CFRunLoopModeUnlock(rlm);
        }
    }
    // run loop 解锁
    __CFRunLoopUnlock(rl);
}

void CFRunLoopRemoveTimer(CFRunLoopRef rl, CFRunLoopTimerRef rlt, CFStringRef modeName)
{
    CHECK_FOR_FORK();
    __CFRunLoopLock(rl);
    if (modeName == kCFRunLoopCommonModes)
    {
        if (NULL != rl->_commonModeItems && CFSetContainsValue(rl->_commonModeItems, rlt))
        {
            CFSetRef set = rl->_commonModes ? CFSetCreateCopy(kCFAllocatorSystemDefault, rl->_commonModes) : NULL;
            CFSetRemoveValue(rl->_commonModeItems, rlt);
            if (NULL != set)
            {
                CFTypeRef context[2] = {rl, rlt};
                /* remove new item from all common-modes */
                CFSetApplyFunction(set, (__CFRunLoopRemoveItemFromCommonModes), (void *)context);
                CFRelease(set);
            }
        }
        else
        {
        }
    }
    else
    {
        CFRunLoopModeRef rlm = __CFRunLoopFindMode(rl, modeName, false);
        CFIndex idx = kCFNotFound;
        CFMutableArrayRef timerList = NULL;
        if (NULL != rlm)
        {
            timerList = rlm->_timers;
            if (NULL != timerList)
            {
                idx = CFArrayGetFirstIndexOfValue(timerList, CFRangeMake(0, CFArrayGetCount(timerList)), rlt);
            }
        }
        if (kCFNotFound != idx)
        {
            __CFRunLoopTimerLock(rlt);
            CFSetRemoveValue(rlt->_rlModes, rlm->_name);
            if (0 == CFSetGetCount(rlt->_rlModes))
            {
                rlt->_runLoop = NULL;
            }
            __CFRunLoopTimerUnlock(rlt);
            CFArrayRemoveValueAtIndex(timerList, idx);
            __CFArmNextTimerInMode(rlm, rl);
        }
        if (NULL != rlm)
        {
            __CFRunLoopModeUnlock(rlm);
        }
    }
    __CFRunLoopUnlock(rl);
}

/* CFRunLoopSource */

static Boolean __CFRunLoopSourceEqual(CFTypeRef cf1, CFTypeRef cf2)
{ /* DOES CALLOUT */
    CFRunLoopSourceRef rls1 = (CFRunLoopSourceRef)cf1;
    CFRunLoopSourceRef rls2 = (CFRunLoopSourceRef)cf2;
    if (rls1 == rls2)
        return true;
    if (__CFIsValid(rls1) != __CFIsValid(rls2))
        return false;
    if (rls1->_order != rls2->_order)
        return false;
    if (rls1->_context.version0.version != rls2->_context.version0.version)
        return false;
    if (rls1->_context.version0.hash != rls2->_context.version0.hash)
        return false;
    if (rls1->_context.version0.equal != rls2->_context.version0.equal)
        return false;
    if (0 == rls1->_context.version0.version && rls1->_context.version0.perform != rls2->_context.version0.perform)
        return false;
    if (1 == rls1->_context.version0.version && rls1->_context.version1.perform != rls2->_context.version1.perform)
        return false;
    if (rls1->_context.version0.equal)
        return rls1->_context.version0.equal(rls1->_context.version0.info, rls2->_context.version0.info);
    return (rls1->_context.version0.info == rls2->_context.version0.info);
}

static CFHashCode __CFRunLoopSourceHash(CFTypeRef cf)
{ /* DOES CALLOUT */
    CFRunLoopSourceRef rls = (CFRunLoopSourceRef)cf;
    if (rls->_context.version0.hash)
        return rls->_context.version0.hash(rls->_context.version0.info);
    return (CFHashCode)rls->_context.version0.info;
}

static CFStringRef __CFRunLoopSourceCopyDescription(CFTypeRef cf)
{ /* DOES CALLOUT */
    CFRunLoopSourceRef rls = (CFRunLoopSourceRef)cf;
    CFStringRef result;
    CFStringRef contextDesc = NULL;
    if (NULL != rls->_context.version0.copyDescription)
    {
        contextDesc = rls->_context.version0.copyDescription(rls->_context.version0.info);
    }
    if (NULL == contextDesc)
    {
        void *addr = rls->_context.version0.version == 0 ? (void *)rls->_context.version0.perform : (rls->_context.version0.version == 1 ? (void *)rls->_context.version1.perform : NULL);
#if DEPLOYMENT_TARGET_WINDOWS
        contextDesc = CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<CFRunLoopSource context>{version = %ld, info = %p, callout = %p}"), rls->_context.version0.version, rls->_context.version0.info, addr);
#elif DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
        Dl_info info;
        const char *name = (dladdr(addr, &info) && info.dli_saddr == addr && info.dli_sname) ? info.dli_sname : "???";
        contextDesc = CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<CFRunLoopSource context>{version = %ld, info = %p, callout = %s (%p)}"), rls->_context.version0.version, rls->_context.version0.info, name, addr);
#endif
    }
#if DEPLOYMENT_TARGET_WINDOWS
    result = CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<CFRunLoopSource %p [%p]>{signalled = %s, valid = %s, order = %d, context = %@}"), cf, CFGetAllocator(rls), __CFRunLoopSourceIsSignaled(rls) ? "Yes" : "No", __CFIsValid(rls) ? "Yes" : "No", rls->_order, contextDesc);
#else
    result = CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<CFRunLoopSource %p [%p]>{signalled = %s, valid = %s, order = %ld, context = %@}"), cf, CFGetAllocator(rls), __CFRunLoopSourceIsSignaled(rls) ? "Yes" : "No", __CFIsValid(rls) ? "Yes" : "No", (unsigned long)rls->_order, contextDesc);
#endif
    CFRelease(contextDesc);
    return result;
}

static void __CFRunLoopSourceDeallocate(CFTypeRef cf)
{ /* DOES CALLOUT */
    CFRunLoopSourceRef rls = (CFRunLoopSourceRef)cf;
    CFRunLoopSourceInvalidate(rls);
    if (rls->_context.version0.release)
    {
        rls->_context.version0.release(rls->_context.version0.info);
    }
    pthread_mutex_destroy(&rls->_lock);
    memset((char *)cf + sizeof(CFRuntimeBase), 0, sizeof(struct __CFRunLoopSource) - sizeof(CFRuntimeBase));
}

static const CFRuntimeClass __CFRunLoopSourceClass = {
    _kCFRuntimeScannedObject,
    "CFRunLoopSource",
    NULL, // init
    NULL, // copy
    __CFRunLoopSourceDeallocate,
    __CFRunLoopSourceEqual,
    __CFRunLoopSourceHash,
    NULL, //
    __CFRunLoopSourceCopyDescription};

CFTypeID CFRunLoopSourceGetTypeID(void)
{
    static dispatch_once_t initOnce;
    dispatch_once(&initOnce, ^{
      __kCFRunLoopSourceTypeID = _CFRuntimeRegisterClass(&__CFRunLoopSourceClass);
    });
    return __kCFRunLoopSourceTypeID;
}

CFRunLoopSourceRef CFRunLoopSourceCreate(CFAllocatorRef allocator, CFIndex order, CFRunLoopSourceContext *context)
{
    CHECK_FOR_FORK();
    CFRunLoopSourceRef memory;
    uint32_t size;
    if (NULL == context)
        CRASH("*** NULL context value passed to CFRunLoopSourceCreate(). (%d) ***", -1);

    size = sizeof(struct __CFRunLoopSource) - sizeof(CFRuntimeBase);
    memory = (CFRunLoopSourceRef)_CFRuntimeCreateInstance(allocator, CFRunLoopSourceGetTypeID(), size, NULL);
    if (NULL == memory)
    {
        return NULL;
    }
    __CFSetValid(memory);
    __CFRunLoopSourceUnsetSignaled(memory);
    __CFRunLoopLockInit(&memory->_lock);
    memory->_bits = 0;
    memory->_order = order;
    memory->_runLoops = NULL;
    size = 0;
    switch (context->version)
    {
    case 0:
        size = sizeof(CFRunLoopSourceContext);
        break;
    case 1:
        size = sizeof(CFRunLoopSourceContext1);
        break;
    }
    objc_memmove_collectable(&memory->_context, context, size);
    if (context->retain)
    {
        memory->_context.version0.info = (void *)context->retain(context->info);
    }
    return memory;
}

CFIndex CFRunLoopSourceGetOrder(CFRunLoopSourceRef rls)
{
    CHECK_FOR_FORK();
    __CFGenericValidateType(rls, CFRunLoopSourceGetTypeID());
    return rls->_order;
}

static void __CFRunLoopSourceWakeUpLoop(const void *value, void *context)
{
    CFRunLoopWakeUp((CFRunLoopRef)value);
}

static void __CFRunLoopSourceRemoveFromRunLoop(const void *value, void *context)
{
    CFRunLoopRef rl = (CFRunLoopRef)value;
    CFTypeRef *params = (CFTypeRef *)context;
    CFRunLoopSourceRef rls = (CFRunLoopSourceRef)params[0];
    CFIndex idx;
    if (rl == params[1])
        return;

    // CFRunLoopRemoveSource will lock the run loop while it
    // needs that, but we also lock it out here to keep
    // changes from occurring for this whole sequence.
    __CFRunLoopLock(rl);
    CFArrayRef array = CFRunLoopCopyAllModes(rl);
    for (idx = CFArrayGetCount(array); idx--;)
    {
        CFStringRef modeName = (CFStringRef)CFArrayGetValueAtIndex(array, idx);
        CFRunLoopRemoveSource(rl, rls, modeName);
    }
    CFRunLoopRemoveSource(rl, rls, kCFRunLoopCommonModes);
    __CFRunLoopUnlock(rl);
    CFRelease(array);
    params[1] = rl;
}

void CFRunLoopSourceInvalidate(CFRunLoopSourceRef rls)
{
    CHECK_FOR_FORK();
    __CFGenericValidateType(rls, CFRunLoopSourceGetTypeID());
    __CFRunLoopSourceLock(rls);
    CFRetain(rls);
    if (__CFIsValid(rls))
    {
        CFBagRef rloops = rls->_runLoops;
        __CFUnsetValid(rls);
        __CFRunLoopSourceUnsetSignaled(rls);
        if (NULL != rloops)
        {
            // To avoid A->B, B->A lock ordering issues when coming up
            // towards the run loop from a source, the source has to be
            // unlocked, which means we have to protect from object
            // invalidation.
            rls->_runLoops = NULL; // transfer ownership to local stack
            __CFRunLoopSourceUnlock(rls);
            CFTypeRef params[2] = {rls, NULL};
            CFBagApplyFunction(rloops, (__CFRunLoopSourceRemoveFromRunLoop), params);
            CFRelease(rloops);
            __CFRunLoopSourceLock(rls);
        }
        /* for hashing- and equality-use purposes, can't actually release the context here */
    }
    __CFRunLoopSourceUnlock(rls);
    CFRelease(rls);
}

Boolean CFRunLoopSourceIsValid(CFRunLoopSourceRef rls)
{
    CHECK_FOR_FORK();
    __CFGenericValidateType(rls, CFRunLoopSourceGetTypeID());
    return __CFIsValid(rls);
}

void CFRunLoopSourceGetContext(CFRunLoopSourceRef rls, CFRunLoopSourceContext *context)
{
    CHECK_FOR_FORK();
    __CFGenericValidateType(rls, CFRunLoopSourceGetTypeID());
    CFAssert1(0 == context->version || 1 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0 or 1", __PRETTY_FUNCTION__);
    CFIndex size = 0;
    switch (context->version)
    {
    case 0:
        size = sizeof(CFRunLoopSourceContext);
        break;
    case 1:
        size = sizeof(CFRunLoopSourceContext1);
        break;
    }
    memmove(context, &rls->_context, size);
}

void CFRunLoopSourceSignal(CFRunLoopSourceRef rls)
{
    CHECK_FOR_FORK();
    __CFRunLoopSourceLock(rls);
    if (__CFIsValid(rls))
    {
        __CFRunLoopSourceSetSignaled(rls);
    }
    __CFRunLoopSourceUnlock(rls);
}

Boolean CFRunLoopSourceIsSignalled(CFRunLoopSourceRef rls)
{
    CHECK_FOR_FORK();
    __CFRunLoopSourceLock(rls);
    Boolean ret = __CFRunLoopSourceIsSignaled(rls) ? true : false;
    __CFRunLoopSourceUnlock(rls);
    return ret;
}

CF_PRIVATE void _CFRunLoopSourceWakeUpRunLoops(CFRunLoopSourceRef rls)
{
    CFBagRef loops = NULL;
    __CFRunLoopSourceLock(rls);
    if (__CFIsValid(rls) && NULL != rls->_runLoops)
    {
        loops = CFBagCreateCopy(kCFAllocatorSystemDefault, rls->_runLoops);
    }
    __CFRunLoopSourceUnlock(rls);
    if (loops)
    {
        CFBagApplyFunction(loops, __CFRunLoopSourceWakeUpLoop, NULL);
        CFRelease(loops);
    }
}

/* CFRunLoopObserver */

static CFStringRef __CFRunLoopObserverCopyDescription(CFTypeRef cf)
{ /* DOES CALLOUT */
    CFRunLoopObserverRef rlo = (CFRunLoopObserverRef)cf;
    CFStringRef result;
    CFStringRef contextDesc = NULL;
    if (NULL != rlo->_context.copyDescription)
    {
        contextDesc = rlo->_context.copyDescription(rlo->_context.info);
    }
    if (!contextDesc)
    {
        contextDesc = CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<CFRunLoopObserver context %p>"), rlo->_context.info);
    }
#if DEPLOYMENT_TARGET_WINDOWS
    result = CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<CFRunLoopObserver %p [%p]>{valid = %s, activities = 0x%x, repeats = %s, order = %d, callout = %p, context = %@}"), cf, CFGetAllocator(rlo), __CFIsValid(rlo) ? "Yes" : "No", rlo->_activities, __CFRunLoopObserverRepeats(rlo) ? "Yes" : "No", rlo->_order, rlo->_callout, contextDesc);
#elif DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
    void *addr = rlo->_callout;
    Dl_info info;
    const char *name = (dladdr(addr, &info) && info.dli_saddr == addr && info.dli_sname) ? info.dli_sname : "???";
    result = CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<CFRunLoopObserver %p [%p]>{valid = %s, activities = 0x%lx, repeats = %s, order = %ld, callout = %s (%p), context = %@}"), cf, CFGetAllocator(rlo), __CFIsValid(rlo) ? "Yes" : "No", (long)rlo->_activities, __CFRunLoopObserverRepeats(rlo) ? "Yes" : "No", (long)rlo->_order, name, addr, contextDesc);
#endif
    CFRelease(contextDesc);
    return result;
}

static void __CFRunLoopObserverDeallocate(CFTypeRef cf)
{ /* DOES CALLOUT */
    CFRunLoopObserverRef rlo = (CFRunLoopObserverRef)cf;
    CFRunLoopObserverInvalidate(rlo);
    pthread_mutex_destroy(&rlo->_lock);
}

static const CFRuntimeClass __CFRunLoopObserverClass = {
    0,
    "CFRunLoopObserver",
    NULL, // init
    NULL, // copy
    __CFRunLoopObserverDeallocate,
    NULL,
    NULL,
    NULL, //
    __CFRunLoopObserverCopyDescription};

CFTypeID CFRunLoopObserverGetTypeID(void)
{
    static dispatch_once_t initOnce;
    dispatch_once(&initOnce, ^{
      __kCFRunLoopObserverTypeID = _CFRuntimeRegisterClass(&__CFRunLoopObserverClass);
    });
    return __kCFRunLoopObserverTypeID;
}

CFRunLoopObserverRef CFRunLoopObserverCreate(CFAllocatorRef allocator, CFOptionFlags activities, Boolean repeats, CFIndex order, CFRunLoopObserverCallBack callout, CFRunLoopObserverContext *context)
{
    CHECK_FOR_FORK();
    CFRunLoopObserverRef memory;
    UInt32 size;
    size = sizeof(struct __CFRunLoopObserver) - sizeof(CFRuntimeBase);
    memory = (CFRunLoopObserverRef)_CFRuntimeCreateInstance(allocator, CFRunLoopObserverGetTypeID(), size, NULL);
    if (NULL == memory)
    {
        return NULL;
    }
    __CFSetValid(memory);
    __CFRunLoopObserverUnsetFiring(memory);
    if (repeats)
    {
        __CFRunLoopObserverSetRepeats(memory);
    }
    else
    {
        __CFRunLoopObserverUnsetRepeats(memory);
    }
    __CFRunLoopLockInit(&memory->_lock);
    memory->_runLoop = NULL;
    memory->_rlCount = 0;
    memory->_activities = activities;
    memory->_order = order;
    memory->_callout = callout;
    if (context)
    {
        if (context->retain)
        {
            memory->_context.info = (void *)context->retain(context->info);
        }
        else
        {
            memory->_context.info = context->info;
        }
        memory->_context.retain = context->retain;
        memory->_context.release = context->release;
        memory->_context.copyDescription = context->copyDescription;
    }
    else
    {
        memory->_context.info = 0;
        memory->_context.retain = 0;
        memory->_context.release = 0;
        memory->_context.copyDescription = 0;
    }
    return memory;
}

static void _runLoopObserverWithBlockContext(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *opaqueBlock)
{
    typedef void (^observer_block_t)(CFRunLoopObserverRef observer, CFRunLoopActivity activity);
    observer_block_t block = (observer_block_t)opaqueBlock;
    block(observer, activity);
}

CFRunLoopObserverRef CFRunLoopObserverCreateWithHandler(CFAllocatorRef allocator, CFOptionFlags activities, Boolean repeats, CFIndex order,
                                                        void (^block)(CFRunLoopObserverRef observer, CFRunLoopActivity activity))
{
    CFRunLoopObserverContext blockContext;
    blockContext.version = 0;
    blockContext.info = (void *)block;
    blockContext.retain = (const void *(*)(const void *info))_Block_copy;
    blockContext.release = (void (*)(const void *info))_Block_release;
    blockContext.copyDescription = NULL;
    return CFRunLoopObserverCreate(allocator, activities, repeats, order, _runLoopObserverWithBlockContext, &blockContext);
}

CFOptionFlags CFRunLoopObserverGetActivities(CFRunLoopObserverRef rlo)
{
    CHECK_FOR_FORK();
    __CFGenericValidateType(rlo, CFRunLoopObserverGetTypeID());
    return rlo->_activities;
}

CFIndex CFRunLoopObserverGetOrder(CFRunLoopObserverRef rlo)
{
    CHECK_FOR_FORK();
    __CFGenericValidateType(rlo, CFRunLoopObserverGetTypeID());
    return rlo->_order;
}

Boolean CFRunLoopObserverDoesRepeat(CFRunLoopObserverRef rlo)
{
    CHECK_FOR_FORK();
    __CFGenericValidateType(rlo, CFRunLoopObserverGetTypeID());
    return __CFRunLoopObserverRepeats(rlo);
}

void CFRunLoopObserverInvalidate(CFRunLoopObserverRef rlo)
{ /* DOES CALLOUT */
    CHECK_FOR_FORK();
    __CFGenericValidateType(rlo, CFRunLoopObserverGetTypeID());
    __CFRunLoopObserverLock(rlo);
    CFRetain(rlo);
    if (__CFIsValid(rlo))
    {
        CFRunLoopRef rl = rlo->_runLoop;
        void *info = rlo->_context.info;
        rlo->_context.info = NULL;
        __CFUnsetValid(rlo);
        if (NULL != rl)
        {
            // To avoid A->B, B->A lock ordering issues when coming up
            // towards the run loop from an observer, it has to be
            // unlocked, which means we have to protect from object
            // invalidation.
            CFRetain(rl);
            __CFRunLoopObserverUnlock(rlo);
            // CFRunLoopRemoveObserver will lock the run loop while it
            // needs that, but we also lock it out here to keep
            // changes from occurring for this whole sequence.
            __CFRunLoopLock(rl);
            CFArrayRef array = CFRunLoopCopyAllModes(rl);
            for (CFIndex idx = CFArrayGetCount(array); idx--;)
            {
                CFStringRef modeName = (CFStringRef)CFArrayGetValueAtIndex(array, idx);
                CFRunLoopRemoveObserver(rl, rlo, modeName);
            }
            CFRunLoopRemoveObserver(rl, rlo, kCFRunLoopCommonModes);
            __CFRunLoopUnlock(rl);
            CFRelease(array);
            CFRelease(rl);
            __CFRunLoopObserverLock(rlo);
        }
        if (NULL != rlo->_context.release)
        {
            rlo->_context.release(info); /* CALLOUT */
        }
    }
    __CFRunLoopObserverUnlock(rlo);
    CFRelease(rlo);
}

Boolean CFRunLoopObserverIsValid(CFRunLoopObserverRef rlo)
{
    CHECK_FOR_FORK();
    return __CFIsValid(rlo);
}

void CFRunLoopObserverGetContext(CFRunLoopObserverRef rlo, CFRunLoopObserverContext *context)
{
    CHECK_FOR_FORK();
    __CFGenericValidateType(rlo, CFRunLoopObserverGetTypeID());
    CFAssert1(0 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);
    *context = rlo->_context;
}

#pragma mark -
#pragma mark CFRunLoopTimer

static CFStringRef __CFRunLoopTimerCopyDescription(CFTypeRef cf)
{ /* DOES CALLOUT */
    CFRunLoopTimerRef rlt = (CFRunLoopTimerRef)cf;
    CFStringRef contextDesc = NULL;
    if (NULL != rlt->_context.copyDescription)
    {
        contextDesc = rlt->_context.copyDescription(rlt->_context.info);
    }
    if (NULL == contextDesc)
    {
        contextDesc = CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL, CFSTR("<CFRunLoopTimer context %p>"), rlt->_context.info);
    }
    void *addr = (void *)rlt->_callout;
    char libraryName[2048];
    char functionName[2048];
    void *functionPtr = NULL;
    libraryName[0] = '?';
    libraryName[1] = '\0';
    functionName[0] = '?';
    functionName[1] = '\0';
    CFStringRef result = CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL,
                                                  CFSTR("<CFRunLoopTimer %p [%p]>{valid = %s, firing = %s, interval = %0.09g, tolerance = %0.09g, next fire date = %0.09g (%0.09g @ %lld), callout = %s (%p / %p) (%s), context = %@}"),
                                                  cf,
                                                  CFGetAllocator(rlt),
                                                  __CFIsValid(rlt) ? "Yes" : "No",
                                                  __CFRunLoopTimerIsFiring(rlt) ? "Yes" : "No",
                                                  rlt->_interval,
                                                  rlt->_tolerance,
                                                  rlt->_nextFireDate,
                                                  rlt->_nextFireDate - CFAbsoluteTimeGetCurrent(),
                                                  rlt->_fireTSR,
                                                  functionName,
                                                  addr,
                                                  functionPtr,
                                                  libraryName,
                                                  contextDesc);
    CFRelease(contextDesc);
    return result;
}

static void __CFRunLoopTimerDeallocate(CFTypeRef cf)
{ /* DOES CALLOUT */
    // CFLog(6, CFSTR("__CFRunLoopTimerDeallocate(%p)"), cf);
    CFRunLoopTimerRef rlt = (CFRunLoopTimerRef)cf;
    __CFRunLoopTimerSetDeallocating(rlt);
    CFRunLoopTimerInvalidate(rlt); /* DOES CALLOUT */
    CFRelease(rlt->_rlModes);
    rlt->_rlModes = NULL;
    pthread_mutex_destroy(&rlt->_lock);
}

static const CFRuntimeClass __CFRunLoopTimerClass = {
    0,
    "CFRunLoopTimer",
    NULL, // init
    NULL, // copy
    __CFRunLoopTimerDeallocate,
    NULL, // equal
    NULL,
    NULL, //
    __CFRunLoopTimerCopyDescription};

CFTypeID CFRunLoopTimerGetTypeID(void)
{
    static dispatch_once_t initOnce;
    dispatch_once(&initOnce, ^{
      __kCFRunLoopTimerTypeID = _CFRuntimeRegisterClass(&__CFRunLoopTimerClass);
    });
    return __kCFRunLoopTimerTypeID;
}

/*
 CFRunLoopTimerCreate 用函数回调创建一个新的 CFRunLoopTimer 对象，有 7 个参数，我们首先看一下各个参数的含义。
 allocator：用于为新对象分配内存的分配器。传递 NULL 或 kCFAllocatorDefault 以使用当前的默认分配器。
 fireDate：计时器应首先触发的时间。如果有实现原因，可以通过计时器对触发日期的精细精度（最多为亚毫秒）进行调整。
 interval：计时器的触发间隔。如果为 0 或负数，计时器将触发一次，然后自动失效。如果存在实施原因，
            可以通过计时器对间隔的精细精度（最多为亚毫秒）进行调整。
 flags：目前被忽略。传递 0 以获得将来的兼容性。
 order：优先级索引，指示运行循环计时器的处理顺序。运行循环计时器当前忽略此参数。传递 0。
 callout：计时器触发时调用的回调函数。
 context：保存运行循环计时器的上下文信息的结构。该函数将信息从结构中复制出来，
        因此上下文所指向的内存不需要在函数调用之后继续存在。
        如果回调函数不需要上下文的信息指针来跟踪状态，则可以为 NULL。
*/
CFRunLoopTimerRef CFRunLoopTimerCreate(CFAllocatorRef allocator,
                                       CFAbsoluteTime fireDate,
                                       CFTimeInterval interval,
                                       CFOptionFlags flags,
                                       CFIndex order,
                                       CFRunLoopTimerCallBack callout,
                                       CFRunLoopTimerContext *context)
{
    CHECK_FOR_FORK();
    // 如果 interval 为 NaN，则 crash
    if (isnan(interval))
    {
        CRSetCrashLogMessage("NaN was used as an interval for a CFRunLoopTimer");
        HALT;
    }
    // struct __CFRunLoopTimer 结构体指针
    CFRunLoopTimerRef memory;
    UInt32 size;
    // 计算 __CFRunLoopTimer 结构除 CFRuntimeBase base 字段之外的内存空间长度
    size = sizeof(struct __CFRunLoopTimer) - sizeof(CFRuntimeBase);
    // 创建 __CFRunLoopTimer 实例并返回其指针
    memory = (CFRunLoopTimerRef)_CFRuntimeCreateInstance(allocator, CFRunLoopTimerGetTypeID(), size, NULL);
    if (NULL == memory)
    {
        return NULL;
    }
    // 设置 memory 的 _cfinfo 字段的值
    __CFSetValid(memory);
    // 设置 memory 的 _cfinfo 字段的值，表示当前 timer 还没有开始执行
    __CFRunLoopTimerUnsetFiring(memory);
    // 初始化 _lock 为互斥递归锁
    __CFRunLoopLockInit(&memory->_lock);
    // _runLoop 置为 NULL
    memory->_runLoop = NULL;
    // 为 timer 的 _rlModes 申请空间
    memory->_rlModes = CFSetCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeSetCallBacks);
    // 优先级，等下把 timer 添加到 mode 的时候会用到
    memory->_order = order;
    // 如果 interval 小于 0.0，则置为 0
    if (interval < 0.0)
        interval = 0.0;
    // 时间间隔赋值
    memory->_interval = interval;
    memory->_tolerance = 0.0;
    // #define TIMER_DATE_LIMIT    4039289856.0
    if (TIMER_DATE_LIMIT < fireDate)
        fireDate = TIMER_DATE_LIMIT;
    // 下次触发的时间
    memory->_nextFireDate = fireDate;
    memory->_fireTSR = 0ULL;
    uint64_t now2 = mach_absolute_time();
    CFAbsoluteTime now1 = CFAbsoluteTimeGetCurrent();
    if (fireDate < now1)
    {
        memory->_fireTSR = now2;
    }
    else if (TIMER_INTERVAL_LIMIT < fireDate - now1)
    {
        memory->_fireTSR = now2 + __CFTimeIntervalToTSR(TIMER_INTERVAL_LIMIT);
    }
    else
    {
        memory->_fireTSR = now2 + __CFTimeIntervalToTSR(fireDate - now1);
    }
    // 回调函数
    memory->_callout = callout;

    // 如果 context 不为 NULL
    if (NULL != context)
    {
        // 是否 retain info
        if (context->retain)
        {
            memory->_context.info = (void *)context->retain(context->info);
        }
        else
        {
            memory->_context.info = context->info;
        }
        // 进行赋值
        memory->_context.retain = context->retain;
        memory->_context.release = context->release;
        memory->_context.copyDescription = context->copyDescription;
    }
    else
    {
        // 置 0
        memory->_context.info = 0;
        memory->_context.retain = 0;
        memory->_context.release = 0;
        memory->_context.copyDescription = 0;
    }
    return memory;
}

static void _runLoopTimerWithBlockContext(CFRunLoopTimerRef timer, void *opaqueBlock)
{
    typedef void (^timer_block_t)(CFRunLoopTimerRef timer);
    timer_block_t block = (timer_block_t)opaqueBlock;
    block(timer);
}

CFRunLoopTimerRef CFRunLoopTimerCreateWithHandler(CFAllocatorRef allocator, CFAbsoluteTime fireDate, CFTimeInterval interval, CFOptionFlags flags, CFIndex order,
                                                  void (^block)(CFRunLoopTimerRef timer))
{

    CFRunLoopTimerContext blockContext;
    blockContext.version = 0;
    blockContext.info = (void *)block;
    blockContext.retain = (const void *(*)(const void *info))_Block_copy;
    blockContext.release = (void (*)(const void *info))_Block_release;
    blockContext.copyDescription = NULL;
    return CFRunLoopTimerCreate(allocator, fireDate, interval, flags, order, _runLoopTimerWithBlockContext, &blockContext);
}

CFAbsoluteTime CFRunLoopTimerGetNextFireDate(CFRunLoopTimerRef rlt)
{
    CHECK_FOR_FORK();
    CF_OBJC_FUNCDISPATCHV(CFRunLoopTimerGetTypeID(), CFAbsoluteTime, (NSTimer *)rlt, _cffireTime);
    __CFGenericValidateType(rlt, CFRunLoopTimerGetTypeID());
    CFAbsoluteTime at = 0.0;
    __CFRunLoopTimerLock(rlt);
    __CFRunLoopTimerFireTSRLock();
    if (__CFIsValid(rlt))
    {
        at = rlt->_nextFireDate;
    }
    __CFRunLoopTimerFireTSRUnlock();
    __CFRunLoopTimerUnlock(rlt);
    return at;
}

void CFRunLoopTimerSetNextFireDate(CFRunLoopTimerRef rlt, CFAbsoluteTime fireDate)
{
    CHECK_FOR_FORK();
    if (!__CFIsValid(rlt))
        return;
    if (TIMER_DATE_LIMIT < fireDate)
        fireDate = TIMER_DATE_LIMIT;
    uint64_t nextFireTSR = 0ULL;
    uint64_t now2 = mach_absolute_time();
    CFAbsoluteTime now1 = CFAbsoluteTimeGetCurrent();
    if (fireDate < now1)
    {
        nextFireTSR = now2;
    }
    else if (TIMER_INTERVAL_LIMIT < fireDate - now1)
    {
        nextFireTSR = now2 + __CFTimeIntervalToTSR(TIMER_INTERVAL_LIMIT);
    }
    else
    {
        nextFireTSR = now2 + __CFTimeIntervalToTSR(fireDate - now1);
    }
    __CFRunLoopTimerLock(rlt);
    if (NULL != rlt->_runLoop)
    {
        CFIndex cnt = CFSetGetCount(rlt->_rlModes);
        STACK_BUFFER_DECL(CFTypeRef, modes, cnt);
        CFSetGetValues(rlt->_rlModes, (const void **)modes);
        // To avoid A->B, B->A lock ordering issues when coming up
        // towards the run loop from a source, the timer has to be
        // unlocked, which means we have to protect from object
        // invalidation, although that's somewhat expensive.
        for (CFIndex idx = 0; idx < cnt; idx++)
        {
            CFRetain(modes[idx]);
        }
        CFRunLoopRef rl = (CFRunLoopRef)CFRetain(rlt->_runLoop);
        __CFRunLoopTimerUnlock(rlt);
        __CFRunLoopLock(rl);
        for (CFIndex idx = 0; idx < cnt; idx++)
        {
            CFStringRef name = (CFStringRef)modes[idx];
            modes[idx] = __CFRunLoopFindMode(rl, name, false);
            CFRelease(name);
        }
        __CFRunLoopTimerFireTSRLock();
        rlt->_fireTSR = nextFireTSR;
        rlt->_nextFireDate = fireDate;
        for (CFIndex idx = 0; idx < cnt; idx++)
        {
            CFRunLoopModeRef rlm = (CFRunLoopModeRef)modes[idx];
            if (rlm)
            {
                __CFRepositionTimerInMode(rlm, rlt, true);
            }
        }
        __CFRunLoopTimerFireTSRUnlock();
        for (CFIndex idx = 0; idx < cnt; idx++)
        {
            __CFRunLoopModeUnlock((CFRunLoopModeRef)modes[idx]);
        }
        __CFRunLoopUnlock(rl);
        // This is setting the date of a timer, not a direct
        // interaction with a run loop, so we'll do a wakeup
        // (which may be costly) for the caller, just in case.
        // (And useful for binary compatibility with older
        // code used to the older timer implementation.)
        if (rl != CFRunLoopGetCurrent())
            CFRunLoopWakeUp(rl);
        CFRelease(rl);
    }
    else
    {
        __CFRunLoopTimerFireTSRLock();
        rlt->_fireTSR = nextFireTSR;
        rlt->_nextFireDate = fireDate;
        __CFRunLoopTimerFireTSRUnlock();
        __CFRunLoopTimerUnlock(rlt);
    }
}

CFTimeInterval CFRunLoopTimerGetInterval(CFRunLoopTimerRef rlt)
{
    CHECK_FOR_FORK();
    CF_OBJC_FUNCDISPATCHV(CFRunLoopTimerGetTypeID(), CFTimeInterval, (NSTimer *)rlt, timeInterval);
    __CFGenericValidateType(rlt, CFRunLoopTimerGetTypeID());
    return rlt->_interval;
}

Boolean CFRunLoopTimerDoesRepeat(CFRunLoopTimerRef rlt)
{
    CHECK_FOR_FORK();
    __CFGenericValidateType(rlt, CFRunLoopTimerGetTypeID());
    return (0.0 < rlt->_interval);
}

CFIndex CFRunLoopTimerGetOrder(CFRunLoopTimerRef rlt)
{
    CHECK_FOR_FORK();
    __CFGenericValidateType(rlt, CFRunLoopTimerGetTypeID());
    return rlt->_order;
}

void CFRunLoopTimerInvalidate(CFRunLoopTimerRef rlt)
{ /* DOES CALLOUT */
    CHECK_FOR_FORK();
    CF_OBJC_FUNCDISPATCHV(CFRunLoopTimerGetTypeID(), void, (NSTimer *)rlt, invalidate);
    __CFGenericValidateType(rlt, CFRunLoopTimerGetTypeID());
    __CFRunLoopTimerLock(rlt);
    if (!__CFRunLoopTimerIsDeallocating(rlt))
    {
        CFRetain(rlt);
    }
    if (__CFIsValid(rlt))
    {
        CFRunLoopRef rl = rlt->_runLoop;
        void *info = rlt->_context.info;
        rlt->_context.info = NULL;
        __CFUnsetValid(rlt);
        if (NULL != rl)
        {
            CFIndex cnt = CFSetGetCount(rlt->_rlModes);
            STACK_BUFFER_DECL(CFStringRef, modes, cnt);
            CFSetGetValues(rlt->_rlModes, (const void **)modes);
            // To avoid A->B, B->A lock ordering issues when coming up
            // towards the run loop from a source, the timer has to be
            // unlocked, which means we have to protect from object
            // invalidation, although that's somewhat expensive.
            for (CFIndex idx = 0; idx < cnt; idx++)
            {
                CFRetain(modes[idx]);
            }
            CFRetain(rl);
            __CFRunLoopTimerUnlock(rlt);
            // CFRunLoopRemoveTimer will lock the run loop while it
            // needs that, but we also lock it out here to keep
            // changes from occurring for this whole sequence.
            __CFRunLoopLock(rl);
            for (CFIndex idx = 0; idx < cnt; idx++)
            {
                CFRunLoopRemoveTimer(rl, rlt, modes[idx]);
            }
            CFRunLoopRemoveTimer(rl, rlt, kCFRunLoopCommonModes);
            __CFRunLoopUnlock(rl);
            for (CFIndex idx = 0; idx < cnt; idx++)
            {
                CFRelease(modes[idx]);
            }
            CFRelease(rl);
            __CFRunLoopTimerLock(rlt);
        }
        if (NULL != rlt->_context.release)
        {
            rlt->_context.release(info); /* CALLOUT */
        }
    }
    __CFRunLoopTimerUnlock(rlt);
    if (!__CFRunLoopTimerIsDeallocating(rlt))
    {
        CFRelease(rlt);
    }
}

Boolean CFRunLoopTimerIsValid(CFRunLoopTimerRef rlt)
{
    CHECK_FOR_FORK();
    CF_OBJC_FUNCDISPATCHV(CFRunLoopTimerGetTypeID(), Boolean, (NSTimer *)rlt, isValid);
    __CFGenericValidateType(rlt, CFRunLoopTimerGetTypeID());
    return __CFIsValid(rlt);
}

void CFRunLoopTimerGetContext(CFRunLoopTimerRef rlt, CFRunLoopTimerContext *context)
{
    CHECK_FOR_FORK();
    __CFGenericValidateType(rlt, CFRunLoopTimerGetTypeID());
    CFAssert1(0 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);
    *context = rlt->_context;
}

CFTimeInterval CFRunLoopTimerGetTolerance(CFRunLoopTimerRef rlt)
{
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
    CHECK_FOR_FORK();
    CF_OBJC_FUNCDISPATCHV(CFRunLoopTimerGetTypeID(), CFTimeInterval, (NSTimer *)rlt, tolerance);
    __CFGenericValidateType(rlt, CFRunLoopTimerGetTypeID());
    return rlt->_tolerance;
#else
    return 0.0;
#endif
}

void CFRunLoopTimerSetTolerance(CFRunLoopTimerRef rlt, CFTimeInterval tolerance)
{
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
    CHECK_FOR_FORK();
    CF_OBJC_FUNCDISPATCHV(CFRunLoopTimerGetTypeID(), void, (NSTimer *)rlt, setTolerance
                          : tolerance);
    __CFGenericValidateType(rlt, CFRunLoopTimerGetTypeID());
    /*
     * dispatch rules:
     *
     * For the initial timer fire at 'start', the upper limit to the allowable
     * delay is set to 'leeway' nanoseconds. For the subsequent timer fires at
     * 'start' + N * 'interval', the upper limit is MIN('leeway','interval'/2).
     */
    if (rlt->_interval > 0)
    {
        rlt->_tolerance = MIN(tolerance, rlt->_interval / 2);
    }
    else
    {
        // Tolerance must be a positive value or zero
        if (tolerance < 0)
            tolerance = 0.0;
        rlt->_tolerance = tolerance;
    }
#endif
}
