/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientValidation.h"

#include "ClientPrefs.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::ContentPrincipalInfo;
using mozilla::ipc::PrincipalInfo;

bool ClientIsValidPrincipalInfo(const PrincipalInfo& aPrincipalInfo) {
  // Ideally we would verify that the source process has permission to
  // create a window or worker with the given principal, but we don't
  // currently have any such restriction in place.  Instead, at least
  // verify the PrincipalInfo is an expected type and has a parsable
  // origin/spec.
  switch (aPrincipalInfo.type()) {
    // Any system and null principal is acceptable.
    case PrincipalInfo::TSystemPrincipalInfo:
    case PrincipalInfo::TNullPrincipalInfo: {
      return true;
    }

    default: { break; }
  }

  // Windows and workers should not have expanded URLs, etc.
  return false;
}

bool ClientIsValidCreationURL(const PrincipalInfo& aPrincipalInfo,
                              const nsACString& aURL) {
  switch (aPrincipalInfo.type()) {
    case PrincipalInfo::TContentPrincipalInfo: {
      // Any origin can create an about:blank or about:srcdoc Client.
      if (aURL.LowerCaseEqualsLiteral("about:blank") ||
          aURL.LowerCaseEqualsLiteral("about:srcdoc")) {
        return true;
      }
      // Otherwise don't support this URL type in the clients sub-system for
      // now.  This will exclude a variety of internal browser clients, but
      // currently we don't need to support those.  This function can be
      // expanded to handle more cases as necessary.
      return false;
    }
    case PrincipalInfo::TNullPrincipalInfo: {
      // A wide variety of clients can have a null principal.  For example,
      // sandboxed iframes can have a normal content URL.  For now allow
      // any parsable URL for null principals.  This is relatively safe since
      // null principals have unique origins and won't most ClientManagerService
      // queries anyway.
      return true;
    }
    default: { break; }
  }

  // Clients (windows/workers) should never have an expanded principal type.
  return false;
}

}  // namespace dom
}  // namespace mozilla
