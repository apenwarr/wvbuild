
PATCHES=$(sort $(filter-out %~,$(filter-out %/CVS,$(wildcard $(addsuffix /*,$(filter-out %/CVS,$(wildcard patches/*)))))))
PACKAGES=$(sort $(filter-out %/CVS,$(wildcard sources/*)))
BUILDDIR=build
ports=$(TOPDIR)/ports/$1/build

.PHONY: default clean unpack patch prepare compile diff
.SILENT: .stamp-unpack clean diff

default: compile

unpack: .stamp-unpack

patch: .stamp-patch

prepare: .stamp-prepare

compile: .stamp-compile

-include .patches.deps

.stamp-unpack: $(PACKAGES) $(PATCHES) Makefile \
	$(addprefix ../,$(addsuffix /.stamp-compile,$(DEPENDS))) \
	$(shell test -d $(BUILDDIR) || rm -f .stamp-*)
	rm -rf $(BUILDDIR) .stamp-compile
	mkdir $(BUILDDIR)
	cd $(BUILDDIR) && for RPMFILE in $(patsubst sources/%,%,$(filter %.rpm,$(PACKAGES))); do \
		echo "*** Unpacking $$RPMFILE..."; \
		rpm2cpio "../sources/$$RPMFILE" | cpio -id; \
	done
	cd $(BUILDDIR) && for TARFILE in $(patsubst sources/%,%,$(filter %.tar.bz2,$(PACKAGES))); do \
		echo "*** Unpacking $$TARFILE..."; \
		tar jxf "../sources/$$TARFILE"; \
	done
	cd $(BUILDDIR) && for TARFILE in $(patsubst sources/%,%,$(filter %.tar.Z %.tgz %.tar.gz,$(PACKAGES))); do \
		echo "*** Unpacking $$TARFILE..."; \
		tar zxf "../sources/$$TARFILE"; \
	done
	cd $(BUILDDIR) && for ZIPFILE in $(patsubst sources/%,%,$(filter %.zip,$(PACKAGES))); do \
		echo "*** Unpacking $$ZIPFILE..."; \
		unzip "../sources/$$ZIPFILE"; \
	done
	$(unpack)
	touch $@

.stamp-patch: .stamp-unpack
	@rm -f .stamp-unpack .stamp-prepare .stamp-compile
	@# Here we generate .patches.deps
	@rm -f .patches.deps.tmp
	@for i in $(PATCHES); do \
		echo "$$i:" >> .patches.deps.tmp; \
	done
	@echo '.stamp-unpack: $(PATCHES)' >> .patches.deps.tmp
	@if ! diff .patches.deps.tmp .patches.deps 2>/dev/null >/dev/null; then\
		mv .patches.deps.tmp .patches.deps; \
	else \
		rm -f .patches.deps.tmp; \
	fi
	@echo "*** Patching..."
	@for PATCH in $(patsubst patches/%,%,$(PATCHES)); do \
		echo "*** Applying patch $$PATCH..."; \
		patch -d $(BUILDDIR)/$$(echo $$PATCH | cut -d/ -f1) \
			-N -p1 < patches/$$PATCH || exit 1; \
	done
	@touch .stamp-unpack $@

.stamp-prepare: .stamp-patch
	@rm -f .stamp-compile
	@echo "*** Preparing..."
	$(prepare)
	@touch $@

.stamp-compile: .stamp-prepare
	@echo "*** Compiling..."
	$(compile)
	@touch $@

.ports.deps: Makefile
	@echo ports/$(PORT): $(addprefix ports/,$(DEPENDS)) >$@

clean:
	echo "*** Cleaning $(shell basename $$PWD)..."
	rm -rf $(wildcard $(BUILDDIR) .patches.deps .build_orig .stamp-*)

diff:
	rm -f .patches.deps
	$(MAKE) BUILDDIR='.build_orig' patch ;\
	cd $(BUILDDIR) \
	&& for SYMLINK in *; do \
		if [ ! -L "$$SYMLINK" ]; then continue; fi; \
		echo "*** Creating 99_$$SYMLINK.diff..." ;\
		$(MAKE) -C .. 99_$$SYMLINK.diff ; \
	done
	rm -rf .build_orig

99_%.diff:
	@cd $(BUILDDIR) \
	&& diff -rpuN --exclude .depend --exclude '*~' \
			--exclude '*.orig' --exclude '*.rej' \
			--exclude '*.l[oa]' --exclude '*.lai' \
			--exclude '*.o' \
			--exclude '.deps' --exclude '.libs' \
			$(patsubst %,--exclude '%',$(EXCLUDE)) \
		'../.build_orig/$(notdir $*)' '$(notdir $*)' \
		| perl -MSys::Hostname -MPOSIX -npe 'BEGIN{$$date=POSIX::strftime("%Y-%m-%d",localtime);($$u,$$n)=(getpwuid$$>)[0,6];$$n=~s/,.*//;$$h=hostname;print("$$date  $$n <$$u\@$$h>\n\n\t* Please describe your patch.\n\n")}' \
		>| '../99_$(notdir $*).diff' \
		|| true

