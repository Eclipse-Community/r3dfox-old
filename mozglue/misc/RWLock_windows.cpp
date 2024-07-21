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
  InitializeCriticalSection(&mLock);
}

mozilla::detail::RWLockImpl::~RWLockImpl() {
  DeleteCriticalSection(&mLock);
}

bool mozilla::detail::RWLockImpl::tryReadLock() {
  EnterCriticalSection(&mLock);
  if (mWriterThreadId == 0 || mWriterThreadId == GetCurrentThreadId()) {
    ++mReaderCount;
    LeaveCriticalSection(&mLock);
    return true;
  }
  LeaveCriticalSection(&mLock);
  return false;
}

void mozilla::detail::RWLockImpl::readLock() {
  EnterCriticalSection(&mLock);
  while (mWriterThreadId != 0 && mWriterThreadId != GetCurrentThreadId()) {
    LeaveCriticalSection(&mLock);
    Sleep(0);
    EnterCriticalSection(&mLock);
  }
  ++mReaderCount;
  LeaveCriticalSection(&mLock);
}

void mozilla::detail::RWLockImpl::readUnlock() {
  EnterCriticalSection(&mLock);
  --mReaderCount;
  LeaveCriticalSection(&mLock);
}

bool mozilla::detail::RWLockImpl::tryWriteLock() {
  EnterCriticalSection(&mLock);
  if (mReaderCount == 0 && mWriterThreadId == 0) {
    mWriterThreadId = GetCurrentThreadId();
    LeaveCriticalSection(&mLock);
    return true;
  }
  LeaveCriticalSection(&mLock);
  return false;
}

void mozilla::detail::RWLockImpl::writeLock() {
  EnterCriticalSection(&mLock);
  while (mReaderCount != 0 || (mWriterThreadId != 0 && mWriterThreadId != GetCurrentThreadId())) {
    LeaveCriticalSection(&mLock);
    Sleep(0);
    EnterCriticalSection(&mLock);
  }
  mWriterThreadId = GetCurrentThreadId();
  LeaveCriticalSection(&mLock);
}

void mozilla::detail::RWLockImpl::writeUnlock() {
  EnterCriticalSection(&mLock);
  mWriterThreadId = 0;
  LeaveCriticalSection(&mLock);
}
