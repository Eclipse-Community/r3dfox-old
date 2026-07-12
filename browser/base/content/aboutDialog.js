/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from aboutDialog-appUpdater.js */

// Services = object with smart getters for common XPCOM services
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

function init(aEvent) {
  if (aEvent.target != document)
    return;

  var distroId = Services.prefs.getCharPref("distribution.id", "");
  if (distroId) {
    var distroString = distroId;

    var distroVersion = Services.prefs.getCharPref("distribution.version", "");
    if (distroVersion) {
      distroString += " - " + distroVersion;
    }

    var distroIdField = document.getElementById("distributionId");
    distroIdField.value = distroString;
    distroIdField.style.display = "block";

    var distroAbout = Services.prefs.getStringPref("distribution.about", "");
    if (distroAbout) {
      var distroField = document.getElementById("distribution");
      distroField.value = distroAbout;
      distroField.style.display = "block";
    }
  }

  // Show a release notes link if we have a URL.
  let relNotesLink = document.getElementById("releasenotes");
  let relNotesPrefType = Services.prefs.getPrefType("app.releaseNotesURL");
  if (relNotesPrefType != Services.prefs.PREF_INVALID) {
    let relNotesURL = Services.urlFormatter.formatURLPref("app.releaseNotesURL");
    if (relNotesURL != "about:blank") {
      relNotesLink.href = relNotesURL;
      relNotesLink.hidden = false;
    }
  }

  if (AppConstants.MOZ_UPDATER) {
    gAppUpdater = new appUpdater({ buttonAutoFocus: true });

    let channelLabel = document.getElementById("currentChannel");
    let currentChannelText = document.getElementById("currentChannelText");
    channelLabel.value = UpdateUtils.UpdateChannel;
    if (/^release($|\-)/.test(channelLabel.value))
        currentChannelText.hidden = true;
  }
  if (AppConstants.platform == "macosx") {
    // it may not be sized at this point, and we need its width to calculate its position
    window.sizeToContent();
    window.moveTo((screen.availWidth / 2) - (window.outerWidth / 2), screen.availHeight / 5);
  }
}
