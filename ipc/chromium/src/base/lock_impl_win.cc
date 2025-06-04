/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lock_impl.h"

namespace base {
namespace internal {

LockImpl::LockImpl() {
  // The second parameter is the spin count, for short-held locks it avoid the
  // contending thread from going to sleep which helps performance greatly.
  ::InitializeCriticalSectionAndSpinCount(&native_handle_, 2000);
}

LockImpl::~LockImpl() {
  ::DeleteCriticalSection(&native_handle_);
}

bool LockImpl::Try() {
  if (::TryEnterCriticalSection(&native_handle_) != FALSE) {
    return true;
  }
  return false;
}

void LockImpl::Lock() {
  ::EnterCriticalSection(&native_handle_);
}

void LockImpl::Unlock() {
  ::LeaveCriticalSection(&native_handle_);
}

}  // namespace internal
}  // namespace base
