ROOT= ..
include ${ROOT}/mk/hdr.mk
include ${ROOT}/mk/ixp.mk

TARG =	libixp
HFILES = ixp_fcall.h

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

ixp_fcall.h: fcall.h fcall.h.nounion ${ROOT}/config.mk
	cat fcall.h${FCALL_H_VERSION} > ixp_fcall.h
	cp -f ixp_fcall.h ${ROOT}/include/
