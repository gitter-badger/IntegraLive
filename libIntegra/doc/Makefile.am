MAINTAINERCLEANFILES=Makefile.in documentation.doxygen

CLEANFILES=doxygen-build.stamp

DOX=documentation.doxygen

EXTRA_DIST=

INSTIMAGES=html/doxygen.png

DOC_STAMPS=html-build.stamp

DOC_DIR=$(HTML_DIR)

all-local: doxygen-build.stamp

doxygen-build.stamp: $(DOX) ../Integra/integra.h
		@echo '*** Running doxygen ***'
		doxygen $(DOX)
		touch doxygen-build.stamp

clean-local:
		rm -f *~ *.bak $(DOC_STAMPS) || true
		if test -d html; then rm -fr html; fi
		if test -d latex; then rm -fr latex; fi
		if test -d man; then rm -fr man; fi

distclean-local: clean
		rm -f *.stamp || true
		if test -d html; then rm -rf html; fi

