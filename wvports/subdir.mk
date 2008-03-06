
PORTS:=$(patsubst wvports/%/Makefile,%,$(wildcard wvports/*/Makefile))
PORTS_PARALLEL:=xplc

.PHONY: ports ports-info mrclean wvports/clean FORCE

ports: $(addprefix wvports/,$(PORTS))

ports-info:
	@for PORT in $(PORTS); do \
		echo -e "\n=== $$PORT ==="; \
		if [ -f wvports/$$PORT/copyright ]; then \
			cat wvports/$$PORT/copyright; \
		fi; \
		echo Source tarballs:; \
		ls wvports/$$PORT/sources | grep -v ^CVS$$ \
			| sort | while read; do \
			echo - $$REPLY; \
		done; \
	done

wvports/clean: $(addsuffix /clean,$(addprefix wvports/,$(PORTS)))

wvports/%: wvports/%/Makefile FORCE
	$(call make_subdir,wvports/$*,,$(if $(filter $*,$(PORTS_PARALLEL)),,-j1))
	$($@-postcmd)
	$(update_lib)

wvports/%/clean: wvports/%/Makefile FORCE
	@$(MAKE) -C wvports/$* clean --no-print-directory

define wvports/dbus-postcmd
	cp -f wvports/dbus/build/dbus/dbus/.libs/libdbus-1.a lib/
endef

define wvports/xplc-postcmd
	cp -f wvports/xplc/build/xplc/libxplc.$(DLLEXT) lib/
	cp -f wvports/xplc/build/xplc/libxplc-cxx.a lib/
endef

define wvports/openssl-postcmd
	cp -f wvports/openssl/build/openssl/libssl.$(DLLEXT) lib/
	cp -f wvports/openssl/build/openssl/libcrypto.$(DLLEXT) lib/
endef

wvports/%/.ports.deps: wvports/%/Makefile
	@echo "--> Computing dependencies for wvports/$*..."
	@$(MAKE) -C wvports/$* --no-print-directory $(notdir $@) PORT=$*

-include $(patsubst %,wvports/%/.ports.deps,$(PORTS))

