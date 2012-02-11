
################################################################################
################################################################################

# Makefile
# hash module build system
#
# $Id: Makefile 25 2006-11-09 20:10:35Z rdent $

################################################################################
## compiler eratta

CC		= gcc
CFLAGS		= -g -Wall
LDFLAGS		=
MKFILE		= Makefile

################################################################################
## utils

CP		= cp
RM		= rm -f
MKDIR		= mkdir
MAKE		= make
INSTALL		= install-sh

################################################################################
## build dirs

VPATH		= .
srcdir		= .
testdir		= ./test

################################################################################
## install dirs

prefix		= .
exec_prefix	= $(prefix)
bindir		= $(exec_prefix)/bin
sbindir		= $(exec_prefix)/sbin
libexecdir	= $(exec_prefix)/libexec
datadir		= $(prefix)/share
libdir		= $(prefix)/lib
includedir	= $(prefix)/include
logfiledir	= $(prefix)/logs

################################################################################
## install

iuser		= nobody
igroup		= nogroup
iexec		= 755
idata		= 644

################################################################################
## in-code options

################################################################################
## files

allobj = hash.o strkey.o intkey.o treekey.o bufkey.o

################################################################################
## make

.PHONY : all
all :
	@echo $(allobj) | xargs $(MAKE)
	@echo done.

.PHONY : clean
clean :
	$(MAKE) -C test clean
	@-$(RM) $(allobj)
	@echo done.

.PHONY : install
install :
	@echo done.

.PHONY : test
test :
	$(MAKE) -C test test
	@echo done.

################################################################################
## compile

$(allobj) : %.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -I. -c $<

################################################################################
################################################################################

