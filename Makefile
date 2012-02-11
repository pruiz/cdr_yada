
################################################################################
################################################################################
# Makefile for cdr_yada
#
# $Id: Makefile 1037 2008-01-15 21:08:10Z grizz $

################################################################################
# variables

ASTERISK_SRC=..
YADA_PATH=/usr/local

UFP_LDADD=hash/hash.o hash/bufkey.o

CC=gcc
LD=gcc -shared -Xlinker -x
CFLAGS=-Wall -g
CPPFLAGS=$(DEFINES) $(INCLUDES)
LDFLAGS=-L$(YADA_PATH)/lib

BUILDREV=008

DEFINES=-D_GNU_SOURCE -DBUILDREV=$(BUILDREV)
INCLUDES=-I$(ASTERISK_SRC) -I$(ASTERISK_SRC)/include -I$(YADA_PATH)/include -Ihash

DISTDIR=cdr_yada-$(BUILDREV)
DISTFILES=Makefile README COPYING cdr_yada.c cdr_yada.conf.sample hash

################################################################################
# build

.PHONY : all clean dist distsign modules install

all : cdr_yada.so

clean :
	-rm -rf cdr_yada.so cdr_yada.o $(DISTDIR) $(DISTDIR).tar.*
	$(MAKE) -C hash clean

install :
	@echo
	@echo Copy cdr_yada.so to your asterisk module directory [default
	@echo /usr/lib/asterisk/modules].
	@echo Copy cdr_yada.conf.sample as cdr_yada.conf into your asterisk
	@echo config directory [default /etc/asterisk] and edit it.
	@echo
	@echo see the file README for details
	@echo

modules:
	$(MAKE) -C hash CFLAGS="-fPIC $(CFLAGS)"

dist : clean
	rm -rf $(DISTDIR)
	mkdir -p $(DISTDIR)
	cp -r $(DISTFILES) $(DISTDIR)
	-find $(DISTDIR) -type d -name .svn -exec rm -rf {} \;
	tar chof - $(DISTDIR) | gzip -c > $(DISTDIR).tar.gz
	tar chof - $(DISTDIR) | bzip2 -9 -c > $(DISTDIR).tar.bz2

distsign: dist
	gpg -b --armor $(DISTDIR).tar.gz
	gpg -b --armor $(DISTDIR).tar.bz2
	md5sum $(DISTDIR).tar.gz $(DISTDIR).tar.bz2

cdr_yada.so : cdr_yada.o modules
	$(LD) cdr_yada.o $(LDFLAGS) -lyada $(UFP_LDADD) -o $@

cdr_yada.o : cdr_yada.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -fPIC -c cdr_yada.c -o $@

################################################################################
################################################################################


