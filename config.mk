# Customize below to fit your system

# paths
PREFIX = /usr/local
BIN = ${PREFIX}/bin
MAN = ${PREFIX}/share/man
ETC = ${PREFIX}/etc
LIBDIR = ${PREFIX}/lib
INCLUDE = ${PREFIX}/include

# Includes and libs
INCS = -I. -I${ROOT}/include -I${INCLUDE} -I/usr/include
LIBS = -L/usr/lib -lc

# Flags
CFLAGS = -g -Wall ${INCS} -DVERSION=\"${VERSION}\"
LDFLAGS = -g ${LIBS}

# Compiler
CC = cc -c
# Linker (Under normal circumstances, this should *not* be 'ld')
LD = cc
# Other
AR = ar crs
#AR = sh -c 'ar cr "$$@" && ranlib "$$@"'

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS} -R${PREFIX}/lib
#LDFLAGS += -lsocket -lnsl
#CFLAGS += -xtarget=ultra
#FCALL_H_VERSION=.nounion
