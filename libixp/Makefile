ROOT= ..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/ixp.mk

TARG =	libixp
FCALL_H = $(ROOT)/include/ixp_fcall.h
HFILES = $(FCALL_H)

OBJ =	client    \
	convert   \
	error     \
	intmap    \
	message   \
	request   \
	rpc       \
	server    \
	socket    \
	thread    \
	transport \
	util

include ${ROOT}/mk/lib.mk

$(FCALL_H):  fcall.h${FCALL_H_VERSION} ${ROOT}/config.mk
	echo HEADER $$($(CLEANNAME) $(BASE)/$(FCALL_H)) from fcall.h${FCALL_H_VERSION}
	cp fcall.h${FCALL_H_VERSION} $(FCALL_H)

