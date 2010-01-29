# This top-level makefile will automatically build a copy of wvstreams using
# the versions openssl, etc. in wvports.

include config.mk

#SUBDIRS=retchmail wvdial wvstreams wvtftp
WVSTREAMSDIR=$(if $(MYHOST),$(MYHOST),wvstreams)
SUBDIRS=$(WVSTREAMSDIR)

.PHONY: default clean mrclean check FORCE $(SUBDIRS)
default: $(SUBDIRS)

ifeq ($(WV_BUILD_MINGW),1)
DLLEXT=a
update_lib:=true
else
ifeq "$(shell uname -s)" "Darwin"
DLLEXT=dylib
update_lib:=$(warning FIXME: I don't know how to generate symlinks for shared libraries.)
else
DLLEXT=so
update_lib=PATH=${PATH}:/sbin ldconfig -N lib
endif
endif

define make_subdir
	@echo
	@echo "--> Making $(if $2,$2 in )$(if $1,$1,$@)..."
	@+$(MAKE) -C $(if $1,$1,$@) --no-print-directory $3 $2
endef

include wvports/subdir.mk

export LD_LIBRARY_PATH:=$(PWD)/lib:$(LD_LIBRARY_PATH)
#export PKG_CONFIG_PATH:=$(PWD)/wvstreams/pkgconfig:$(PKG_CONFIG_PATH)
#export WVSTREAMS:=$(PWD)/wvstreams

clean: $(addsuffix /clean,$(SUBDIRS))

%/clean: FORCE
	$(call make_subdir,$*,clean)

mrclean: clean wvports/clean
	$(call make_subdir,$(WVSTREAMSDIR),realclean)

check: $(addsuffix /check,$(WVSTREAMSDIR))

%/check: % FORCE
	$(call make_subdir,$*,test)

nitlog planit:
	$(error These projects are PHP, go do something sensible instead!)

replytolist:
	$(error I don't think I'll ever be smart enough to build this.)

retchmail: $(WVSTREAMSDIR)
	ln -sf ../wvver.h ../wvstreams/wvrules.mk $@
	$(call make_subdir)

schedulator:
	$(error I don't know (yet!) how to build $@...)

twc: $(WVSTREAMSDIR)
	$(error I don't know (yet!) how to build $@...)

unikonf:
	$(error I don't know (yet!) how to build $@...)

unity:
	$(error I don't know (yet!) how to build $@...)

wvdial: $(WVSTREAMSDIR)
	ln -sf ../wvver.h ../wvstreams/wvrules.mk $@
	$(call make_subdir)

ifeq ($(WV_BUILD_MINGW),1)
$(WVSTREAMSDIR): wvports/win32api
endif
$(WVSTREAMSDIR): wvports/zlib wvports/openssl wvports/dbus
	$(call make_subdir)

wvsync: $(WVSTREAMSDIR)
	$(error I don't know (yet!) how to build $@...)

wvtftp: $(WVSTREAMSDIR)
	cd $@ && cmake .
	$(call make_subdir)

zen:
	$(error I don't know (yet!) how to build $@...)

