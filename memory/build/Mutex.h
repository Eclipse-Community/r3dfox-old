/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Mutex_h
#define Mutex_h

#if defined(XP_WIN)
#include <windows.h>
#elif defined(XP_DARWIN)
#include <libkern/OSAtomic.h>
#else
#include <pthread.h>
#endif
#include "mozilla/GuardObjects.h"

// Mutexes based on spinlocks.  We can't use normal pthread spinlocks in all
// places, because they require malloc()ed memory, which causes bootstrapping
// issues in some cases.  We also can't use constructors, because for statics,
// they would fire after the first use of malloc, resetting the locks.
struct Mutex {
#if defined(XP_WIN)
  SRWLOCK mMutex;
#elif defined(XP_DARWIN)
  OSSpinLock mMutex;
#else
  pthread_mutex_t mMutex;
#endif

  // Initializes a mutex. Returns whether initialization succeeded.
  inline bool Init() {
#if defined(XP_WIN)
    InitializeSRWLock(&mMutex);
#elif defined(XP_DARWIN)
    mMutex = OS_SPINLOCK_INIT;
#elif defined(XP_LINUX) && !defined(ANDROID)
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
      return false;
    }
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
    if (pthread_mutex_init(&mMutex, &attr) != 0) {
      pthread_mutexattr_destroy(&attr);
      return false;
    }
    pthread_mutexattr_destroy(&attr);
#else
    if (pthread_mutex_init(&mMutex, nullptr) != 0) {
      return false;
    }
#endif
    return true;
  }

  inline void Lock() {
#if defined(XP_WIN)
    AcquireSRWLockExclusive(&mMutex);
#elif defined(XP_DARWIN)
    OSSpinLockLock(&mMutex);
#else
    pthread_mutex_lock(&mMutex);
#endif
  }

  inline void Unlock() {
#if defined(XP_WIN)
    ReleaseSRWLockExclusive(&mMutex);
#elif defined(XP_DARWIN)
    OSSpinLockUnlock(&mMutex);
#else
    pthread_mutex_unlock(&mMutex);
#endif
  }
};

struct MOZ_RAII MutexAutoLock {
  explicit MutexAutoLock(Mutex& aMutex MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mMutex(aMutex) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    mMutex.Lock();
  }

  ~MutexAutoLock() { mMutex.Unlock(); }

 private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;
  Mutex& mMutex;
};

#endif
