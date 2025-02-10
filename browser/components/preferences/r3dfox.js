/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

var gR3dfoxPane = {
  init() {},
};

Preferences.addAll([
  { id: "widget.non-native-theme.enabled", type: "bool",  inverted: true },
  { id: "widget.native-controls.scrollbar-style", type: "int" },
  { id: "widget.non-native-theme.scrollbar.style", type: "int" },
  { id: "widget.native-controls.override-win-version", type: "int" },
  { id: "r3dfox.colors", type: "int" },
  { id: "r3dfox.customizations.enabled", type: "bool" },
  { id: "r3dfox.drag-space.enabled", type: "bool" },
  { id: "r3dfox.backgrounds.enabled", type: "bool" },
  { id: "r3dfox.transparent.menubar", type: "bool" },
  { id: "r3dfox.translucent.navbar", type: "bool" },
  { id: "r3dfox.aero.fog", type: "int" },
  { id: "r3dfox.caption.text.color", type: "int" },
  { id: "browser.urlbar.oneOffsInstant", type: "bool" },
  { id: "r3dfox.view.image", type: "bool" },
  { id: "browser.e10s.disabled", type: "bool" },
  { id: "security.csp.enable", type: "bool",  inverted: true },
  { id: "network.stricttransportsecurity.enabled", type: "bool",  inverted: true },
  { id: "accessibility.force_disabled", type: "int" },
  { id: "browser.urlbar.secondaryActions.switchToTab", type: "bool" },
  { id: "gfx.dwrite.enabled", type: "bool",  inverted: true },
]);
