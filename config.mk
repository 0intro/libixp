# Customize below to fit your system

COMPONENTS = \
	libixp \
	libixp_pthread

# Paths
PREFIX = /usr/local
  BIN = $(PREFIX)/bin
  MAN = $(PREFIX)/share/man
  ETC = $(PREFIX)/etc
  LIBDIR = $(PREFIX)/lib
  INCLUDE = $(PREFIX)/include

# Includes and libs
INCPATH = .:$(ROOT)/include:$(INCLUDE):/usr/include
LIBS = -L/usr/lib -lc

# Flags
include $(ROOT)/mk/gcc.mk
CFLAGS += $(DEBUGCFLAGS) -O0 $(INCS)
LDFLAGS = -g $(LDLIBS) $(LIBS)

# Compiler, Linker. Linker should usually *not* be ld.
CC = cc -c
LD = cc
# Archiver
AR = ar crs
#AR = sh -c 'ar cr "$$@" && ranlib "$$@"'

# Solaris
#CFLAGS = -fast $(INCS)
#LDFLAGS = $(LIBS) -R$(PREFIX)/lib -lsocket -lnsl
#CFLAGS += -xtarget=ultra

# Misc
#MAKESO = 1

# Extra Components
IGNORE = \
	libixp_task \
	libixp_rubythread

RUBYINC = -I/usr/local/lib/ruby/1.8/i386-freebsd6
TASKINC = -I$(HOME)/libtask

