
################################################################################
################################################################################

# Makefile
# hash module build system
#
# $Id: Makefile 14 2006-03-14 15:50:05Z grizz $

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

alltest = t_bufkey t_strkey t_treekey
allobj = t_bufkey.o t_strkey.o t_treekey.o
reqobj = ../hash.o

################################################################################
## make

.PHONY : all
all :
	@echo $(allobj) | xargs $(MAKE)
	@echo done.

.PHONY : clean
clean :
	@-$(RM) $(allobj) $(alltest)
	@echo done.

.PHONY : test
test : $(alltest)
	for each in $(alltest); do \
	  /bin/ls -A1 ../ | ./$$each; \
	done;
	@echo done.

################################################################################
## compile

$(allobj) : %.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -I.. -c $<

$(alltest) : t_% : t_%.o ../%.o
	$(CC) -o $@ $^ $(reqobj) $(LDFLAGS) $(LIBS)

################################################################################
################################################################################

