all:
	(cd src && $(MAKE))

install:	all
	(cd src && $(MAKE) install DESTDIR=$(DESTDIR))

clean:
	(cd src && $(MAKE) clean)

tests:
	(cd src && $(MAKE) tests)

distclean: 	clean
	rm -f Makefile config.status config.log
	(cd src && $(MAKE) distclean)

docs:
	doxygen Doxyfile
