VERSION = 0.5

COPYRIGHT = ©2010 Kris Maglione

$(ROOT)/include/ixp.h: $(ROOT)/config.mk
CFLAGS += '-DVERSION=\"$(VERSION)\"' '-DCOPYRIGHT=\"$(COPYRIGHT)\"'

