
include ../../config.mk
include ../wvports.mk

define unpack
	ln -s zlib-1.2.3 $(BUILDDIR)/zlib
endef

define prepare
	cd $(BUILDDIR)/zlib && ./configure
endef

define compile
	make -C $(BUILDDIR)/zlib
endef

