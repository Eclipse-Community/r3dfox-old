/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyPreferenceGetter(lazy, "sidebarBackupState", "sidebar.backupState");

export const SidebarManager = {
  init() {
    const featureId = "sidebar";
  },

  getBackupState() {
    try {
      return JSON.parse(lazy.sidebarBackupState);
    } catch (e) {
      Services.prefs.clearUserPref("sidebar.backupState");
      return null;
    }
  },

  setBackupState(state) {
    if (!state) {
      return;
    }
    Services.prefs.setStringPref("sidebar.backupState", JSON.stringify(state));
  },
};

SidebarManager.init();
