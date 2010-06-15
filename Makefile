ROOT=.
include $(ROOT)/mk/hdr.mk

DIRS =	lib	\
	cmd	\
	include	\
	man

doc:
	perl $(ROOT)/util/grepdoc $$(hg manifest | egrep '^(lib|include)') \
		>$(ROOT)/man/targets.mk
	$(MAKE) -Cman

.PHONY: doc
include $(ROOT)/mk/dir.mk

