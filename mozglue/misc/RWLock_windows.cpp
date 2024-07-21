/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XP_WIN
#  error This file should only be compiled on Windows.
#endif

#include "mozilla/PlatformRWLock.h"

mozilla::detail::RWLockImpl::RWLockImpl()
    : mWriterThreadId(0), mReaderCount(0) {
  InitializeCriticalSection(&criticalSection);
}

mozilla::detail::RWLockImpl::~RWLockImpl() {
  DeleteCriticalSection(&criticalSection);
}

bool mozilla::detail::RWLockImpl::tryReadLock() {
  EnterCriticalSection(&criticalSection);
  if (mWriterThreadId == 0 || mWriterThreadId == GetCurrentThreadId()) {
    ++mReaderCount;
    LeaveCriticalSection(&criticalSection);
    return true;
  }
  LeaveCriticalSection(&criticalSection);
  return false;
}

void mozilla::detail::RWLockImpl::readLock() {
  EnterCriticalSection(&criticalSection);
  while (mWriterThreadId != 0 && mWriterThreadId != GetCurrentThreadId()) {
    LeaveCriticalSection(&criticalSection);
    Sleep(0);
    EnterCriticalSection(&criticalSection);
  }
  ++mReaderCount;
  LeaveCriticalSection(&criticalSection);
}

void mozilla::detail::RWLockImpl::readUnlock() {
  EnterCriticalSection(&criticalSection);
  --mReaderCount;
  LeaveCriticalSection(&criticalSection);
}

bool mozilla::detail::RWLockImpl::tryWriteLock() {
  EnterCriticalSection(&criticalSection);
  if (mReaderCount == 0 && mWriterThreadId == 0) {
    mWriterThreadId = GetCurrentThreadId();
    LeaveCriticalSection(&criticalSection);
    return true;
  }
  LeaveCriticalSection(&criticalSection);
  return false;
}

void mozilla::detail::RWLockImpl::writeLock() {
  EnterCriticalSection(&criticalSection);
  while (mReaderCount != 0 || (mWriterThreadId != 0 && mWriterThreadId != GetCurrentThreadId())) {
    LeaveCriticalSection(&criticalSection);
    Sleep(0);
    EnterCriticalSection(&criticalSection);
  }
  mWriterThreadId = GetCurrentThreadId();
  LeaveCriticalSection(&criticalSection);
}

void mozilla::detail::RWLockImpl::writeUnlock() {
  EnterCriticalSection(&criticalSection);
  mWriterThreadId = 0;
  LeaveCriticalSection(&criticalSection);
}
