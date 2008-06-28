
include ../../config.mk
include ../wvports.mk

define unpack
	ln -s argp-standalone-1.3 $(BUILDDIR)/argp
endef

define prepare
	cd build/argp && ./configure
endef

ifeq ($(WV_BUILD_MINGW),1)

# MINGW32 build

endif

define compile
	make -C build/argp
endef
