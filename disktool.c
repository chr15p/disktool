/*
* Copyright (C) 2010 Chris Procter
*
* This file is part of fctool
*
* This copyrighted material is made available to anyone wishing to use,
* modify, copy, or redistribute it subject to the terms and conditions
* of the GNU General Public License v.2.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <getopt.h>
#include <syslog.h>
#include <unistd.h>

#include "directory.h"

//	heading			func			location		printfunc	flags
Column disks[] = {
        {"Address",		&linkvalue,		"device",		&print_field,	RUN},
        {"Device",		&get_dirname,		"hostname",		&print_field,	RUN},
        {"Devno",		&get_file_contents,	"dev",			&print_field,	DEVNO},
        {"Size",		&get_size,		"size",			&print_field,	RUN},
        {"Vendor",		&get_file_contents,	"device/vendor",	&print_field,	RUN},
        {"Model",		&get_file_contents,	"device/model",		&print_field,	RUN},
        {NULL,			&delete,		"device/delete",	NULL,		DELETE},
        {NULL,			NULL,			"note",			&print_field,	RUN},
        {NULL,			NULL,			0,			NULL,		0}
};

//	paths		filter	flags	cols
Filesearch sections[] = {
	{"/sys/block/",	"hd",	HEAD,	disks},
	{"/sys/block/",	"sd",	0,	disks},
	{NULL,		NULL,	0,	NULL}
};

void usage(char* progname)
{
	printf("usage:\n");
	printf("%s:\n",progname);
	printf("\t-n\t\t- display device numbers\n"); 
	printf("\t-d\t\t- cleanup unused devices\n"); 
	printf("\t-h\t\t- print this help message\n"); 
	printf("\n");
	exit(1);
}


static struct option longopts[] = {
  { "delete",  no_argument,      NULL,   'd' },
  { "devno",  no_argument,      NULL,   'n' },
  { "help",  no_argument,      NULL,   'h' },
  { NULL,  0,  NULL,  0 }
};



int main(int argc,char *argv[])
{
	int i=0;
	//int j=0;
	char ch;
	GSList * dev_list=NULL;
	char * format="%-18s ";
	//Filesearch sections[];
	int displayflags=RUN;
	uid_t userid;

	userid=getuid();

	opterr=0;

	while((ch = getopt_long(argc, argv, "+hdn",longopts,NULL)) != -1)
	{
		switch(ch){
			case 'd':
				if(userid==0){
					displayflags|=DELETE;
				}else{
					fprintf(stderr,"you must be root to use the -%c option\n",ch);
				}
				break;
			case 'n':
				displayflags|=DEVNO;
				break;
			case 'h':
			default:
				usage(g_path_get_basename(argv[0]));
				break;
		}
	}


	while(sections[i].path!=NULL){
		get_entities(&dev_list,&(sections[i]),displayflags);

		
		if(sections[i].flags== HEAD){
			print_headers(&(sections[i]),format);
		}
	
		if(dev_list!=NULL){	
			do{
				print_entities((GHashTable *) dev_list->data,&(sections[i]),format);
			}while((dev_list=g_slist_next(dev_list))!=NULL);
		}	
		i++;
	}

	return EXIT_SUCCESS;
}
