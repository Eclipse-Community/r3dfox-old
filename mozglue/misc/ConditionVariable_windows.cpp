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

// Some versions of the Windows SDK have a bug where some interlocked functions
// are not redefined as compiler intrinsics. Fix that for the interlocked
// functions that are used in this file.
#if defined(_MSC_VER) && !defined(InterlockedExchangeAdd)
#define InterlockedExchangeAdd(addend, value) \
  _InterlockedExchangeAdd((volatile long*)(addend), (long)(value))
#endif

#if defined(_MSC_VER) && !defined(InterlockedIncrement)
#define InterlockedIncrement(addend) \
  _InterlockedIncrement((volatile long*)(addend))
#endif

// Windows XP and Server 2003 don't support condition variables natively. The
// NativeImports class is responsible for detecting native support and
// retrieving the appropriate function pointers. It gets instantiated once,
// using a static initializer.
class ConditionVariableNativeImports {
 public:
  ConditionVariableNativeImports() {
    HMODULE kernel32_dll = GetModuleHandle("kernel32.dll");
    MOZ_RELEASE_ASSERT(kernel32_dll != NULL);

#define LOAD_SYMBOL(symbol) loadSymbol(kernel32_dll, #symbol, symbol)
    supported_ = LOAD_SYMBOL(InitializeConditionVariable) &&
                 LOAD_SYMBOL(WakeConditionVariable) &&
                 LOAD_SYMBOL(WakeAllConditionVariable) &&
                 LOAD_SYMBOL(SleepConditionVariableCS);
#undef LOAD_SYMBOL
  }

  inline bool supported() const {
    return supported_;
  }

  void(WINAPI* InitializeConditionVariable)(CONDITION_VARIABLE* ConditionVariable);
  void(WINAPI* WakeAllConditionVariable)(PCONDITION_VARIABLE ConditionVariable);
  void(WINAPI* WakeConditionVariable)(CONDITION_VARIABLE* ConditionVariable);
  BOOL(WINAPI* SleepConditionVariableCS)(CONDITION_VARIABLE* ConditionVariable,
                                         CRITICAL_SECTION* CriticalSection,
                                         DWORD dwMilliseconds);

 private:
  template <typename T>
  inline bool loadSymbol(HMODULE module, const char* name, T& fn) {
    FARPROC ptr = GetProcAddress(module, name);
    if (!ptr)
      return false;

    fn = reinterpret_cast<T>(ptr);
    return true;
  }

  bool supported_;
};

static ConditionVariableNativeImports sNativeImports;

// Wrapper for native condition variable APIs.
struct ConditionVariableNative {
  inline void initialize() {
    sNativeImports.InitializeConditionVariable(&cv_);
  }

  inline void destroy() {
    // Native condition variables don't require cleanup.
  }

  inline void notify_one() { sNativeImports.WakeConditionVariable(&cv_); }

  inline void notify_all() { sNativeImports.WakeAllConditionVariable(&cv_); }

  inline bool wait(CRITICAL_SECTION* cs, DWORD msec) {
    return sNativeImports.SleepConditionVariableCS(&cv_, cs, msec);
  }

 private:
  CONDITION_VARIABLE cv_;
};

// Fast fallback condition variable support for Windows XP and Server 2003.
struct ConditionVariableFallback {
  enum : uint32_t {
    C_SIGNAL = 0,
    C_BROADCAST = 1,
    C_MAX_EVENTS = 2
  };

  void initialize() {
    waitersCount_ = 0;
    waitersCountLock_ = 0;

    events_[C_SIGNAL] = CreateEventW(NULL, FALSE, FALSE, NULL);
    MOZ_RELEASE_ASSERT(events_[C_SIGNAL]);

    events_[C_BROADCAST] = CreateEventW(NULL, TRUE, FALSE, NULL);
    MOZ_RELEASE_ASSERT(events_[C_BROADCAST]);
  }

  void destroy() {
    BOOL r;
    r = CloseHandle(events_[C_SIGNAL]);
    MOZ_RELEASE_ASSERT(r);

    r = CloseHandle(events_[C_BROADCAST]);
    MOZ_RELEASE_ASSERT(r);
  }

  void notify_one() {
    bool haveWaiters;
    lock_();
    haveWaiters = (waitersCount_ > 0);
    unlock_();

    if (haveWaiters) {
      BOOL success = SetEvent(events_[C_SIGNAL]);
      MOZ_RELEASE_ASSERT(success);
    }
  }

  void notify_all() {
    bool haveWaiters;
    lock_();
    haveWaiters = (waitersCount_ > 0);
    unlock_();

    if (haveWaiters) {
      BOOL success = SetEvent(events_[C_BROADCAST]);
      MOZ_RELEASE_ASSERT(success);
    }
  }

  bool wait(CRITICAL_SECTION* userLock, DWORD msec) {
    lock_();
    ++waitersCount_;
    unlock_();

    LeaveCriticalSection(userLock);

    HANDLE handles[C_MAX_EVENTS] = {events_[C_SIGNAL], events_[C_BROADCAST]};
    DWORD waitResult = WaitForMultipleObjects(C_MAX_EVENTS, handles, FALSE, msec);
    MOZ_RELEASE_ASSERT(waitResult == WAIT_OBJECT_0 ||
                       waitResult == WAIT_OBJECT_0 + 1 ||
                       waitResult == WAIT_TIMEOUT);

    lock_();
    --waitersCount_;
    if (waitersCount_ == 0) {
      BOOL success = ResetEvent(events_[C_BROADCAST]);
      MOZ_RELEASE_ASSERT(success);
    }
    unlock_();

    // Reacquire the user mutex.
    EnterCriticalSection(userLock);

    // Return true if woken up, false when timed out.
    if (waitResult == WAIT_TIMEOUT) {
      SetLastError(ERROR_TIMEOUT);
      return false;
    }
    return true;
  }

 private:
  void lock_() {
    for (int spin = 0;; ++spin) {
      if (InterlockedCompareExchange(&waitersCountLock_, 1, 0) == 0)
        return;

      // Backoff: first yield the pipeline a bit, then let the scheduler run.
      if ((spin & 0x3F) == 0)
        SwitchToThread();     // or Sleep(0)
      else
        YieldProcessor();
    }
  }

  void unlock_() {
    InterlockedExchange(&waitersCountLock_, 0);
  }

 private:
  uint32_t waitersCount_ = 0;
  volatile LONG waitersCountLock_ = 0;
  HANDLE events_[C_MAX_EVENTS]{};
};

struct mozilla::detail::ConditionVariableImpl::PlatformData {
  union {
    ConditionVariableNative native;
    ConditionVariableFallback fallback;
  };
};

mozilla::detail::ConditionVariableImpl::ConditionVariableImpl() {
  if (sNativeImports.supported())
    platformData()->native.initialize();
  else
    platformData()->fallback.initialize();
}

void mozilla::detail::ConditionVariableImpl::notify_one() {
  if (sNativeImports.supported())
    platformData()->native.notify_one();
  else
    platformData()->fallback.notify_one();
}

void mozilla::detail::ConditionVariableImpl::notify_all() {
  if (sNativeImports.supported())
    platformData()->native.notify_all();
  else
    platformData()->fallback.notify_all();
}

void mozilla::detail::ConditionVariableImpl::wait(MutexImpl& lock) {
  CRITICAL_SECTION* cs = &lock.platformData()->criticalSection;
  bool r;
  if (sNativeImports.supported())
    r = platformData()->native.wait(cs, INFINITE);
  else
    r = platformData()->fallback.wait(cs, INFINITE);
  MOZ_RELEASE_ASSERT(r);
}

mozilla::detail::CVStatus mozilla::detail::ConditionVariableImpl::wait_for(
    MutexImpl& lock, const mozilla::TimeDuration& rel_time) {
  if (rel_time == mozilla::TimeDuration::Forever()) {
    wait(lock);
    return CVStatus::NoTimeout;
  }

  CRITICAL_SECTION* cs = &lock.platformData()->criticalSection;

  // Note that DWORD is unsigned, so we have to be careful to clamp at 0. If
  // rel_time is Forever, then ToMilliseconds is +inf, which evaluates as
  // greater than UINT32_MAX, resulting in the correct INFINITE wait. We also
  // don't want to round sub-millisecond waits to 0, as that wastes energy (see
  // bug 1437167 comment 6), so we instead round submillisecond waits to 1ms.
  double msecd = rel_time.ToMilliseconds();
  DWORD msec;
  if (msecd < 0.0) {
    msec = 0;
  } else if (msecd > UINT32_MAX) {
    msec = INFINITE;
  } else {
    msec = static_cast<DWORD>(msecd);
    // Round submillisecond waits to 1ms.
    if (msec == 0 && !rel_time.IsZero()) {
      msec = 1;
    }
  }

  BOOL r;
  if (sNativeImports.supported())
    r = platformData()->native.wait(cs, msec);
  else
    r = platformData()->fallback.wait(cs, msec);
  if (r) return CVStatus::NoTimeout;
  MOZ_RELEASE_ASSERT(GetLastError() == ERROR_TIMEOUT);
  return CVStatus::Timeout;
}

mozilla::detail::ConditionVariableImpl::~ConditionVariableImpl() {
  if (sNativeImports.supported())
    platformData()->native.destroy();
  else
    platformData()->fallback.destroy();
}

inline mozilla::detail::ConditionVariableImpl::PlatformData*
mozilla::detail::ConditionVariableImpl::platformData() {
  static_assert(sizeof platformData_ >= sizeof(PlatformData),
                "platformData_ is too small");
  return reinterpret_cast<PlatformData*>(platformData_);
}
