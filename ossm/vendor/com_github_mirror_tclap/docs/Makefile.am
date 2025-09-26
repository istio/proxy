.NOTPARALLEL:

if DOC

all:
	@DOXYGEN@ Doxyfile

manual: manual.xml
	xsltproc -o $(abs_builddir)/manual.html --stringparam html.stylesheet style.css /usr/share/xml/docbook/stylesheet/docbook-xsl/html/docbook.xsl  $(abs_srcdir)/manual.xml
else

all:

endif

EXTRA_DIST = manual.xml
dist_doc_DATA = index.html manual.html style.css
docdir = ${datadir}/doc/${PACKAGE}

install-data-local :
	$(mkdir_p) $(DESTDIR)$(docdir)
	cp -R html $(DESTDIR)$(docdir)

uninstall-local :
	chmod -R o+w $(DESTDIR)$(docdir)/html
	rm -rf $(DESTDIR)$(docdir)/html

dist-hook :
	$(mkdir_p) $(distdir)
	cp -R $(abs_builddir)/html $(distdir)

clean-local:
	$(RM) -rf $(abs_builddir)/html/*
	$(RM) -rf $(abs_builddir)/doxygen_sqlite3.db

