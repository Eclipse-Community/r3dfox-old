/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { UserAgentOverrides } from "resource://gre/modules/UserAgentOverrides.sys.mjs";

export function UAOverridesBootstrapper() {
  this.init();
}

UAOverridesBootstrapper.prototype = {
  init: function uaob_init() {
    Services.obs.addObserver(this, "profile-change-net-teardown");
    UserAgentOverrides.init();
  },

  observe: function uaob_observe(aSubject, aTopic, aData) {
    if (aTopic == "profile-change-net-teardown") {
      Services.obs.removeObserver(this, "profile-change-net-teardown");
      UserAgentOverrides.uninit();
    }
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),
  classID: Components.ID("{965b0ca8-155b-11e7-93ae-92361f002671}"),
};
