
SUBDIRS=zlib openssl dbus win32api

default: all

all:
	for d in $(SUBDIRS); do \
		$(MAKE) -C $$d; \
	done

clean:
	rm -f *~
	for d in $(SUBDIRS); do \
		$(MAKE) -C $$d clean; \
	done
