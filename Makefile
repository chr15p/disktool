# fctool - manage fc devices 
#
# Copyright (C) 2010 Chris Procter
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v.2.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

CC=gcc
CFLAGS=-Wall -g 
EXE=disktool 
OBJ=disktool.o directory.o
HEADERS=directory.h
MANPAGE=fctool.8
INSTALLDIR=$(INSTALL_ROOT)/usr/sbin
MANDIR=$(INSTALL_ROOT)/usr/share/man/man8

COMPILEFLAGS=$(shell pkg-config --cflags --libs glib-2.0)

LINKERFLAGS=$(shell pkg-config --cflags --libs glib-2.0)


all: fctool 

fctool: $(OBJ) $(HEADERS)
	$(CC) $(OBJ) -o $(EXE) $(LINKERFLAGS) $(CFLAGS) $(PPFLAGS) 


.c.o:
	$(CC) $(CFLAGS) -c $< $(COMPILEFLAGS) $(PPFLAGS) 

doc:
	gzip -c $(MANPAGE) >$(MANPAGE).gz

install:
	install -d $(INSTALLDIR) 
	#install -d $(MANDIR) 
	cp $(EXE) $(INSTALLDIR) 
	#cp $(MANPAGE).gz $(MANDIR)


clean:
	-rm $(OBJ)
	-rm $(EXE)
	-rm $(MANPAGE).gz
