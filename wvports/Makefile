
SUBDIRS=zlib xplc openssl dbus

default: all

all:
	for d in $(SUBDIRS); do \
		make -C $$d; \
	done

clean:
	rm -f *~
	for d in $(SUBDIRS); do \
		make -C $$d clean; \
	done
