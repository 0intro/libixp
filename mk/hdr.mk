.SILENT:
.SUFFIXES: .O .o .c .sh .rc .awk .1 .depend .install .uninstall .clean
all:

.c.depend:
	echo MKDEP $<
	${MKDEP} ${CFLAGS} $< >>.depend

.c.o:
	${COMPILE} $@ $<

.o.O:
	${LINK} $@ $<

.c.O:
	${COMPILE} ${<:.c=.o} $<
	${LINK} $@ ${<:.c=.o}

.awk.O:
	echo FILTER ${BASE}$<
	${FILTER} $< >$@
	chmod 0755 $@

.rc.O:
	echo FILTER ${BASE}$<
	${FILTER} $< >$@
	chmod 0755 $@

.sh.O:
	echo FILTER ${BASE}$<
	${FILTER} $< >$@
	chmod 0755 $@

.O.install:
	echo INSTALL $$($(CLEANNAME) ${BASE}$*)
	cp -f $< ${BIN}/$*
	chmod 0755 ${BIN}/$* 
.O.uninstall:
	echo UNINSTALL $$($(CLEANNAME) ${BASE}$*)
	rm -f ${BIN}/$* 

.a.install:
	echo INSTALL $$($(CLEANNAME) ${BASE}$<)
	cp -f $< ${LIBDIR}/$<
	chmod 0644 ${LIBDIR}/$<
.a.uninstall:
	echo UNINSTALL $$($(CLEANNAME) ${BASE}$<)
	rm -f ${LIBDIR}/$<

.h.install:
	echo INSTALL $$($(CLEANNAME) ${BASE}$<)
	cp -f $< ${INCLUDE}/$<
	chmod 0644 ${INCLUDE}/$<
.h.uninstall:
	echo UNINSTALL $$($(CLEANNAME) ${BASE}$<)
	rm -f ${INCLUDE}/$<

.1.install:
	echo INSTALL man $$($(CLEANNAME) $*'(1)')
	${FILTER} $< >${MAN}/man1/$<
	chmod 0644 ${MAN}/man1/$<
.1.uninstall:
	echo UNINSTALL man $$($(CLEANNAME) $*'(1)')
	rm -f ${MAN}/man1/$<

.O.clean:
	rm -f $< || true 2>/dev/null
	rm -f $*.o || true 2>/dev/null
.o.clean:
	rm -f $< || true 2>/dev/null

printinstall:
mkdirs:
clean:
install: printinstall mkdirs
depend: cleandep

FILTER = cat
COMPILE= CC="${CC}" CFLAGS="${CFLAGS}" ${ROOT}/util/compile
LINK= LD="${LD}" LDFLAGS="${LDFLAGS}" ${ROOT}/util/link
CLEANNAME=${ROOT}/util/cleanname

include ${ROOT}/config.mk
CFLAGS += -I$$(echo ${INCPATH}|sed 's/:/ -I/g')

