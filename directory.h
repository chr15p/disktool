/*
* Copyright (C) 2010 Chris Procter
*
* This file is part of disk_admin
*
* This copyrighted material is made available to anyone wishing to use,
* modify, copy, or redistribute it subject to the terms and conditions
* of the GNU General Public License v.2.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef HDIR

#define RUN 1
#define HEAD 2
#define DELETE 4

typedef struct _column {
	char * heading;
	int (*func)(GHashTable* hash,char * path,char * data,int flags);
	char * location;
	void (*printfunc)(GHashTable* device,char * heading,char *loc,char * format,int displayflags);
	int flags;
} Column;

typedef struct _filesearch {
	char * path;
	char * filter;
	int flags;
	Column *cols;
} Filesearch;

extern int get_size(GHashTable *hash,char * path,char * location,int flags);
extern int get_file_contents(GHashTable *hash,char * path,char * location,int flags);
extern int linkvalue(GHashTable *hash,char * path,char * location,int flags);
extern int get_dirname(GHashTable *hash,char * path,char * location,int flags);
extern int delete(GHashTable *hash,char * path,char * location,int flags);

extern int get_entities(GSList ** devlist,Filesearch s[],int displayflags);

//void print_field(GHashTable* hash,char * heading,char * loc,char* format,int flags);
void print_field(GHashTable* hash,char * heading,char * loc,char* format,int flags);
void print_line(GHashTable* hash,char * heading,char * loc,char* format,int flags);
void print_entities(GHashTable* hash,Filesearch s[],char * format);
void print_headers(Filesearch s[],char * format);

#define HDIR 1
#endif
