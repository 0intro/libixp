VERSION = 0.5

$(ROOT)/include/ixp.h: $(ROOT)/config.mk
CFLAGS += '-DVERSION=\"$(VERSION)\"' -D_XOPEN_SOURCE=600

