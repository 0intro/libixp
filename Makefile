# libixp - simple 9P client-/server-library
#   (C)opyright MMIV-MMVII Anselm R. Garbe

include config.mk

SRC = client.c convert.c intmap.c message.c request.c server.c socket.c \
      transport.c util.c
SRCIXPC = ixpc.c
OBJ = ${SRC:.c=.o}
OBJIXPC = ${SRCIXPC:.c=.o}

#all: options libixp.a libixp.so ixpc
all: options libixp.a ixpc

options:
	@echo libixp build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"
	@echo "SOFLAGS  = ${SOFLAGS}"
	@echo "LD       = ${LD}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk ixp.h

libixp.a: ${OBJ}
	@echo AR $@
	@${AR} $@ ${OBJ}
	@${RANLIB} $@

libixp.so: ${OBJ}
	@echo LD $@
	@${LD} ${SOFLAGS} -o $@ ${OBJ}

ixpc: ${OBJIXPC}
	@echo LD $@
	@${LD} -o $@ ${OBJIXPC} ${LDFLAGS} -lixp
	@strip $@

clean:
	@echo cleaning
	@rm -f ixpc libixp.a libixp.so ${OBJ} ${OBJIXPC} libixp-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p libixp-${VERSION}
	@cp -R LICENSE LICENSE.p9p Makefile README config.mk ixp.h ixpc.1 ${SRC} ${SRCIXPC} libixp-${VERSION}
	@tar -cf libixp-${VERSION}.tar libixp-${VERSION}
	@gzip libixp-${VERSION}.tar
	@rm -rf libixp-${VERSION}

install: all
	@echo installing header to ${DESTDIR}${PREFIX}/include
	@mkdir -p ${DESTDIR}${PREFIX}/include
	@cp -f ixp.h ${DESTDIR}${PREFIX}/include
	@chmod 644 ${DESTDIR}${PREFIX}/include/ixp.h
	@echo installing library to ${DESTDIR}${PREFIX}/lib
	@mkdir -p ${DESTDIR}${PREFIX}/lib
	@cp -f libixp.a ${DESTDIR}${PREFIX}/lib
	@chmod 644 ${DESTDIR}${PREFIX}/lib/libixp.a
#	@cp -f libixp.so ${DESTDIR}${PREFIX}/lib/libixp.so.${VERSION}
#	@chmod 755 ${DESTDIR}${PREFIX}/lib/libixp.so.${VERSION}
#	@ln -s libixp.so.${VERSION} ${DESTDIR}${PREFIX}/lib/libixp.so
	@echo installing ixpc to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f ixpc ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/ixpc
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed 's/VERSION/${VERSION}/g' < ixpc.1 > ${DESTDIR}${MANPREFIX}/man1/ixpc.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/ixpc.1

uninstall:
	@echo removing header file from ${DESTDIR}${PREFIX}/include
	@rm -f ${DESTDIR}${PREFIX}/include/ixp.h

	@echo removing library file from ${DESTDIR}${PREFIX}/lib
	@rm -f ${DESTDIR}${PREFIX}/lib/libixp.a
	@echo removing shared object file from ${DESTDIR}${PREFIX}/lib
#	@rm -f ${DESTDIR}${PREFIX}/lib/libixp.so
#	@rm -f ${DESTDIR}${PREFIX}/lib/libixp.so.${VERSION}
	@echo removing ipx client from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/ixpc
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/ixpc.1

.PHONY: all options clean dist install uninstall
