
include ../../config.mk
include ../wvports.mk

define unpack
	ln -s dbus-1.1.2 $(BUILDDIR)/dbus
endef

define prepare
	cd $(BUILDDIR)/dbus && \
		./configure --build=$$(./config.guess) $(CONFIGHOST) \
			--disable-abstract-sockets \
			--disable-x11 \
			--disable-shared \
			--without-pic
endef

define compile
	make -C $(BUILDDIR)/dbus/dbus libdbus-1.la
endef

%.diff:EXCLUDE= Makefile stamp-* messagebus rc.messagebus \
	session.conf system.conf config.h config.log \
	config.status dbus-1.pc Doxyfile libtool data
