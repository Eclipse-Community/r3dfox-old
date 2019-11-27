# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# We need to include the mozilla packaging routines because we are
# very much still dependent on them
include $(MOZILLA_DIR)/toolkit/mozapps/installer/packager.mk

# This is currently only used on Windows, Linux, Solaris, and
# FreeBSD. On other platforms such as Mac, we will fall back
# to the original mozilla packaging.
make-archive:
ifeq (,$(filter SunOS Linux WINNT FreeBSD,$(OS_ARCH)))
	$(MAKE) make-package
else
	$(MAKE) stage-package
	@echo 'Compressing...'
ifeq (WINNT,$(OS_ARCH))
	cd $(DIST); $(CYGWIN_WRAPPER) 7z a -t7z -m0=lzma2 -mx=9 -aoa -bb3 $(PKG_BASENAME).7z $(MOZ_PKG_DIR)
else
	cd $(DIST); XZ_OPT=-9e $(TAR) cfJv $(PKG_BASENAME).tar.xz $(MOZ_PKG_DIR)
endif
endif
