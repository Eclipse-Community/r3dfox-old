# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# NSIS branding defines for nightly builds.
# The official release build branding.nsi is located in other-license/branding/firefox/
# The unofficial build branding.nsi is located in browser/branding/unofficial/

# BrandFullNameInternal is used for some registry and file system values
# instead of BrandFullName and typically should not be modified.
!define BrandFullNameInternal "Nightly"
!define BrandFullName         "Firefox Nightly"
!define CompanyName           "mozilla.org"
!define URLInfoAbout          "https://www.mozilla.org"
!define HelpLink              "https://support.mozilla.org"

!define URLStubDownload32 "https://download.mozilla.org/?os=win&lang=${AB_CD}&product=firefox-nightly-latest"
!define URLStubDownload64 "https://download.mozilla.org/?os=win64&lang=${AB_CD}&product=firefox-nightly-latest"
!define URLManualDownload "https://www.mozilla.org/${AB_CD}/firefox/installer-help/?channel=nightly&installer_lang=${AB_CD}"
!define URLSystemRequirements "https://www.mozilla.org/firefox/system-requirements/"
!define Channel "nightly"
