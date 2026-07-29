/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxSettings_h
#define mozilla_SandboxSettings_h

namespace mozilla {

// Return the current sandbox level. This is the
// "security.sandbox.content.level" preference, but rounded up to the current
// minimum allowed level.
int GetEffectiveContentSandboxLevel();

#if defined(XP_MACOSX)
int ClampFlashSandboxLevel(const int aLevel);
#endif

#if defined(__OpenBSD__)
bool StartOpenBSDSandbox(GeckoProcessType type);
#endif

}  // namespace mozilla
#endif  // mozilla_SandboxPolicies_h
