/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

#include <float.h>
#include <intrin.h>
#include <stdlib.h>
#include <windows.h>

#include "mozilla/PlatformConditionVariable.h"
#include "mozilla/PlatformMutex.h"
#include "MutexPlatformData_windows.h"

// Windows XP and Server 2003 do not support condition variables natively.
// This implementation always uses the fallback (no native condvars).
// Should be similar enough in performance to native condvars.
// Also helps to stability test this workaround if all operating systems use it.

struct ConditionVariableFallback {
  uint32_t waiting;
  CRITICAL_SECTION lock_waiting;
  enum {
    SIGNAL = 0,
    BROADCAST = 1,
    MAX_EVENTS = 2
  } EVENTS;
  HANDLE events[MAX_EVENTS];
  HANDLE broadcast_block_event;

  void initialize() {
    waiting = 0;
    InitializeCriticalSection(&lock_waiting);

    events[SIGNAL] = CreateEventW(NULL, FALSE, FALSE, NULL);
    events[BROADCAST] = CreateEventW(NULL, TRUE, FALSE, NULL);
    broadcast_block_event = CreateEventW(NULL, TRUE, TRUE, NULL);
  }

  void destroy() {
    DeleteCriticalSection(&lock_waiting);

    CloseHandle(events[SIGNAL]);
    CloseHandle(events[BROADCAST]);
    CloseHandle(broadcast_block_event);
  }

 public:
  void notify_one() {
    EnterCriticalSection(&lock_waiting);
    if (waiting > 0) {
      SetEvent(events[SIGNAL]);
    }
    LeaveCriticalSection(&lock_waiting);
  }

  void notify_all() {
    EnterCriticalSection(&lock_waiting);

    // Protects us from broadcasting if there isn't any thread waiting to
    // open the block gate after this call has closed it.
    if (waiting > 0) {
      // Close block gate
      ResetEvent(broadcast_block_event);
      // Open broadcast gate
      SetEvent(events[BROADCAST]);
    }

    LeaveCriticalSection(&lock_waiting);
  }

  bool wait(CRITICAL_SECTION* userLock, DWORD msec) {
    int result;
    DWORD timeout = msec;

    // Block access if previous broadcast hasn't finished. This should normally
    // not affect total time spent in this function.
    WaitForSingleObject(broadcast_block_event, INFINITE);

    EnterCriticalSection(&lock_waiting);
    waiting++;
    LeaveCriticalSection(&lock_waiting);

    LeaveCriticalSection(userLock);

    result = WaitForMultipleObjects(2, events, FALSE, timeout);

    EnterCriticalSection(&lock_waiting);
    waiting--;

    if (waiting == 0) {
      // We're the last waiter to be notified or to stop waiting.
      // Reset state so next broadcast can proceed.
      ResetEvent(events[BROADCAST]);
      SetEvent(broadcast_block_event);
    }

    LeaveCriticalSection(&lock_waiting);

    EnterCriticalSection(userLock);

    // Return true if woken up, false when timed out.
    if (result == WAIT_TIMEOUT) {
      SetLastError(ERROR_TIMEOUT);
      return false;
    }
    return true;
  }
};

struct mozilla::detail::ConditionVariableImpl::PlatformData {
  ConditionVariableFallback fallback;
};

mozilla::detail::ConditionVariableImpl::ConditionVariableImpl() {
  platformData()->fallback.initialize();
}

void mozilla::detail::ConditionVariableImpl::notify_one() {
  platformData()->fallback.notify_one();
}

void mozilla::detail::ConditionVariableImpl::notify_all() {
  platformData()->fallback.notify_all();
}

void mozilla::detail::ConditionVariableImpl::wait(MutexImpl& lock) {
  CRITICAL_SECTION* cs = &lock.platformData()->criticalSection;
  bool r = platformData()->fallback.wait(cs, INFINITE);
  MOZ_RELEASE_ASSERT(r);
}

mozilla::detail::CVStatus mozilla::detail::ConditionVariableImpl::wait_for(
    MutexImpl& lock, const mozilla::TimeDuration& rel_time) {
  if (rel_time == mozilla::TimeDuration::Forever()) {
    wait(lock);
    return CVStatus::NoTimeout;
  }

  CRITICAL_SECTION* cs = &lock.platformData()->criticalSection;

  // Note that DWORD is unsigned, so we have to be careful to clamp at 0.
  // If rel_time is Forever, then ToMilliseconds is +inf, resulting in INFINITE.
  // Don't round sub-millisecond waits to 0; round them to 1ms instead.
  double msecd = rel_time.ToMilliseconds();
  DWORD msec;
  if (msecd < 0.0) {
    msec = 0;
  } else if (msecd > UINT32_MAX) {
    msec = INFINITE;
  } else {
    msec = static_cast<DWORD>(msecd);
    if (msec == 0 && !rel_time.IsZero()) {
      msec = 1;
    }
  }

  BOOL r = platformData()->fallback.wait(cs, msec) ? TRUE : FALSE;
  if (r) return CVStatus::NoTimeout;
  MOZ_RELEASE_ASSERT(GetLastError() == ERROR_TIMEOUT);
  return CVStatus::Timeout;
}

mozilla::detail::ConditionVariableImpl::~ConditionVariableImpl() {
  platformData()->fallback.destroy();
}

inline mozilla::detail::ConditionVariableImpl::PlatformData*
mozilla::detail::ConditionVariableImpl::platformData() {
  static_assert(sizeof platformData_ >= sizeof(PlatformData),
                "platformData_ is too small");
  return reinterpret_cast<PlatformData*>(platformData_);
}
