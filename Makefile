ROOT=.
include $(ROOT)/mk/hdr.mk
include $(ROOT)/mk/ixp.mk

DIRS =	lib	\
	cmd	\
	include	\
	man

doc:
	perl $(ROOT)/util/grepdoc $$(hg manifest | grep -E '^(lib|include)') \
		>$(ROOT)/man/targets.mk
	$(MAKE) -Cman

deb-dep:
	IFS=', '; \
	apt-get -qq install build-essential $$(sed -n 's/([^)]*)//; s/^Build-Depends: \(.*\)/\1/p' debian/control)

DISTRO = unstable
deb:
	$(ROOT)/util/genchangelog libixp-hg $(VERSION) $(DISTRO)
	dpkg-buildpackage -rfakeroot -b -nc
	[ -d .hg ] && hg revert debian/changelog || true

.PHONY: doc
include $(ROOT)/mk/dir.mk

