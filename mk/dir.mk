dall:
	for i in ${DIRS}; do \
		echo MAKE all ${BASE}$$i/; \
		(cd $$i && ${MAKE} BASE="${BASE}$$i/" all) || exit 1; \
	done
dclean:
	for i in ${DIRS}; do \
		echo MAKE clean ${BASE}$$i/; \
		(cd $$i && ${MAKE} BASE="${BASE}$$i/" clean) || exit 1; \
	done
dinstall:
	for i in ${DIRS}; do \
		echo MAKE install ${BASE}$$i/; \
		(cd $$i && ${MAKE} BASE="${BASE}$$i/" install) || exit 1; \
	done
ddepend:
	for i in ${DIRS}; do \
		echo MAKE depend ${BASE}$$i/; \
		(cd $$i && ${MAKE} BASE="${BASE}$$i/" depend) || exit 1; \
	done

all: dall
clean: dclean
install: dinstall
depend: ddepend
