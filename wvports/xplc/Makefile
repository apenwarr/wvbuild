
include ../../config.mk
include ../wvports.mk

ifeq ($(WV_BUILD_MINGW),1)
TARGETS=libxplc.a libxplc-cxx.a libxplc_s.a
endif

define unpack
	ln -s xplc-0.3.13 $(BUILDDIR)/xplc
endef

define prepare
	cd $(BUILDDIR)/xplc && \
		./configure --build=$$(./config.guess) $(CONFIGHOST)
endef

define compile
	$(MAKE) -C $(BUILDDIR)/xplc $(TARGETS)
endef

