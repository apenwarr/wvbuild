CPPFLAGS += -Iinclude -pipe
ARFLAGS = rs

DEBUG:=$(filter-out no,$(enable_debug))

# for O_LARGEFILE
CXXFLAGS+=${CXXOPTS}
CFLAGS+=${COPTS}
CXXFLAGS+=-D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS+=-D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

ifneq ($(DEBUG),)
CXXFLAGS+=-ggdb -DDEBUG$(if $(filter-out yes,$(DEBUG)), -DDEBUG_$(DEBUG))
CFLAGS+=-ggdb -DDEBUG$(if $(filter-out yes,$(DEBUG)), -DDEBUG_$(DEBUG))
endif

ifeq ("$(enable_debug)", "no")
#CXXFLAGS+=-fomit-frame-pointer
# -DNDEBUG is disabled because we like assert() to crash
#CXXFLAGS+=-DNDEBUG
#CFLAGS+=-DNDEBUG
endif

ifeq ("$(enable_fatal_warnings)", "yes")
CXXFLAGS+=-Werror
# FIXME: not for C, because our only C file, crypto/wvsslhack.c, has
#        a few warnings.
#CFLAGS+=-Werror
endif

ifneq ("$(enable_optimization)", "no")
CXXFLAGS+=-O2
#CXXFLAGS+=-felide-constructors
CFLAGS+=-O2
endif

ifneq ("$(enable_warnings)", "no")
CXXFLAGS+=-Wall -Woverloaded-virtual
CFLAGS+=-Wall
endif

ifeq ("$(enable_testgui)", "no")
WVTESTRUN=env
endif

ifeq ("$(enable_efence)", "yes")
LDLIBS+=-lefence
endif

ifneq ("$(with_bdb)", "no")
  libwvutils.so-LIBS+=-ldb
endif

ifneq ("$(with_qdbm)", "no")
  libwvutils.so-LIBS+=-L. -lqdbm
endif

libwvbase.so-LIBS+=-lxplc-cxx -lm
libwvbase.so:

ifneq ("$(with_openslp)", "no")
  libwvstreams.so: -lslp
endif

ifneq ("$(with_pam)", "no")
  libwvstreams.so: -lpam
endif

