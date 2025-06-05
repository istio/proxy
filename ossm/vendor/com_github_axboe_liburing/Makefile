include Makefile.common

RPMBUILD=$(shell `which rpmbuild >&/dev/null` && echo "rpmbuild" || echo "rpm")

INSTALL=install

default: all

all:
	@$(MAKE) -C src
	@$(MAKE) -C test
	@$(MAKE) -C examples

library:
	@$(MAKE) -C src

.PHONY: all install default clean test library
.PHONY: FORCE cscope

runtests: all
	@$(MAKE) -C test runtests
runtests-loop: all
	@$(MAKE) -C test runtests-loop
runtests-parallel: all
	@$(MAKE) -C test runtests-parallel

config-host.mak: configure
	+@if [ ! -e "$@" ]; then				\
	  echo "Running configure ...";				\
	  ./configure;						\
	else							\
	  echo "$@ is out-of-date, running configure";		\
	  sed -n "/.*Configured with/s/[^:]*: //p" "$@" | sh;	\
	fi

ifneq ($(MAKECMDGOALS),clean)
include config-host.mak
endif

%.pc: %.pc.in config-host.mak $(SPECFILE)
	sed -e "s%@prefix@%$(prefix)%g" \
	    -e "s%@libdir@%$(libdir)%g" \
	    -e "s%@includedir@%$(includedir)%g" \
	    -e "s%@NAME@%$(NAME)%g" \
	    -e "s%@VERSION@%$(VERSION)%g" \
	    $< >$@

install: $(NAME).pc $(NAME)-ffi.pc
	@$(MAKE) -C src install prefix=$(DESTDIR)$(prefix) \
		includedir=$(DESTDIR)$(includedir) \
		libdir=$(DESTDIR)$(libdir) \
		libdevdir=$(DESTDIR)$(libdevdir) \
		relativelibdir=$(relativelibdir)
	$(INSTALL) -D -m 644 $(NAME).pc $(DESTDIR)$(libdevdir)/pkgconfig/$(NAME).pc
	$(INSTALL) -D -m 644 $(NAME)-ffi.pc $(DESTDIR)$(libdevdir)/pkgconfig/$(NAME)-ffi.pc
	$(INSTALL) -m 755 -d $(DESTDIR)$(mandir)/man2
	$(INSTALL) -m 644 man/*.2 $(DESTDIR)$(mandir)/man2
	$(INSTALL) -m 755 -d $(DESTDIR)$(mandir)/man3
	$(INSTALL) -m 644 man/*.3 $(DESTDIR)$(mandir)/man3
	$(INSTALL) -m 755 -d $(DESTDIR)$(mandir)/man7
	$(INSTALL) -m 644 man/*.7 $(DESTDIR)$(mandir)/man7

uninstall:
	@$(MAKE) -C src uninstall prefix=$(DESTDIR)$(prefix) datadir=$(DESTDIR)$(datadir)
	@rm -f $(DESTDIR)$(libdevdir)/pkgconfig/$(NAME).pc
	@rm -f $(DESTDIR)$(libdevdir)/pkgconfig/$(NAME)-ffi.pc
	@rm -rf $(DESTDIR)$(mandir)/man2/io_uring*.2
	@rm -rf $(DESTDIR)$(mandir)/man3/io_uring*.3
	@rm -rf $(DESTDIR)$(mandir)/man7/io_uring*.7

install-tests:
	@$(MAKE) -C test install prefix=$(DESTDIR)$(prefix) datadir=$(DESTDIR)$(datadir)

uninstall-tests:
	@$(MAKE) -C test uninstall prefix=$(DESTDIR)$(prefix) datadir=$(DESTDIR)$(datadir)

clean:
	@rm -f config-host.mak config-host.h cscope.out $(NAME).pc $(NAME)-ffi.pc test/*.dmesg
	@$(MAKE) -C src clean
	@$(MAKE) -C test clean
	@$(MAKE) -C examples clean

cscope:
	@cscope -b -R

tag-archive:
	@git tag $(TAG)

create-archive:
	@git archive --prefix=$(NAME)-$(VERSION)/ -o $(NAME)-$(VERSION).tar.gz $(TAG)
	@echo "The final archive is ./$(NAME)-$(VERSION).tar.gz."

archive: clean tag-archive create-archive

srpm: create-archive
	$(RPMBUILD) --define "_sourcedir `pwd`" --define "_srcrpmdir `pwd`" --nodeps -bs $(SPECFILE)
