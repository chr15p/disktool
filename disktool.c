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
#include <sys/inotify.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "directory.h"

//	heading			func			location		printfunc	flags
Column disks[] = {
        {"Address",		&linkvalue,		"device",		&print_field,	RUN},
        {"Device",		&get_dirname,		"hostname",		&print_field,	RUN},
        {"Devno",		&get_file_contents,	"dev",			&print_field,	DEVNO},
        {"Size",		&get_size,		"size",			&print_field,	RUN},
        {"Mpath",		&get_mpathdev2,		"mpathdev",		&print_field,	MULTIPATH},
        {"DM",			&get_dmdev2,		"dmdev",		&print_field,	MULTIPATH},
        {"DM devno",		&get_dmdevno,		"dmdevno",		&print_field,	MULTIPATH},
        {"Vendor",		&get_file_contents,	"device/vendor",	&print_field,	RUN},
        {"Model",		&get_file_contents,	"device/model",		&print_field,	RUN},
        {"Scheduler",		&get_scheduler,		"queue/scheduler",	&print_field,	RUN},
        {NULL,			&rescan,		"device/rescan",	NULL,		DELETE},
        {NULL,			&delete,		"device/delete",	NULL,		DELETE},
        {NULL,			NULL,			"note",			&print_field,	RUN},
        {NULL,			NULL,			0,			NULL,		0}
};

//	paths		filter	flags	cols
Filesearch sections[] = {
	{"/sys/block/",	"hd",	HEAD|HD,disks},
	{"/sys/block/",	"sd",	SD,	disks},
	{"/sys/block/",	"dm",	DM,	disks},
	{"/sys/block/",	"ram",	RAM,	disks},
	{"/sys/block/",	"loop",	LOOP,	disks},
	{NULL,		NULL,	0,	NULL}
};


void usage(char* progname)
{
	printf("usage:\n");
	printf("%s:\n",progname);
	printf("\t-n\t\t- display device numbers\n"); 
	printf("\t-d\t\t- cleanup unused devices\n"); 
	printf("\t-s\t\t- rescan the scsi busses\n");
	printf("\t-q\t\t- do not display column headers\n");
	printf("\t-m\t\t- display associated multipath devices\n");
	printf("\t-p [seperator]\t- seperator for output\n");
	printf("\t-h\t\t- print this help message\n"); 
	printf("\n");
	exit(1);
}


static struct option longopts[] = {
  { "delete",	no_argument,	NULL,	'd'},
  { "devno",	no_argument,	NULL,	'n'},
  { "scan",	no_argument,	NULL,	's'},
  { "noheaders",	no_argument,	NULL,	'q'},
  { "multipath",	no_argument,	NULL,	'm'},
  { "seperator",	required_argument,	NULL,	'p'},
  { "sd",	no_argument,	NULL,	'z'},
  { "ram",	no_argument,	NULL,	'r'},
  { "loop",	no_argument,	NULL,	'l'},
  { "dm",	no_argument,	NULL,	'a'},
  { "help",	no_argument,	NULL,	'h'},
  { NULL,  0,  NULL,  0 }
};



int main(int argc,char *argv[])
{
	int i=0;
	int j;
	int k;
	char ch;
	GSList * dev_list=NULL;
	char * format="%-16s ";
	//Filesearch sections[];
	int displayflags=RUN|HEAD;
	int filterflags=0;
	uid_t userid;
	int scanflag=0;
	int fd;
	int wd;
	int len;
	char buf[1024];
	time_t tend;
	struct inotify_event *event;
	gint (*sortfunc)(gconstpointer a,gconstpointer b)=&compare_addresses;

	userid=getuid();

	opterr=0;

	while((ch = getopt_long(argc, argv, "+hdnsqmzrlap:",longopts,NULL)) != -1)
	{
		switch(ch){
			case 'd':
				if(userid==0){
					displayflags|=DELETE;
				}else{
					fprintf(stderr,"you must be root to use the -%c option\n",ch);
				}
				break;
			case 's':
				if(userid==0){
					scanflag=1;
				}else{
					fprintf(stderr,"you must be root to use the -%c option\n",ch);
				}
				break;
                        case 'p':
				format=(char *) malloc(strlen(optarg)+3);
				strcpy(format,"%s");
 				strcat(format,optarg);
				//displayflags|=SEPARATOR;
				break;
			case 'n':
				displayflags|=DEVNO;
				break;
			case 'm':
				displayflags|=MULTIPATH;
				break;
			case 'q':
				displayflags &= ~HEAD;
				break;
			case 'z':
				filterflags |= HD|SD;
				break;
			case 'r':
				filterflags |= RAM;
				break;
			case 'l':
				filterflags |= LOOP;
				break;
			case 'a':
				filterflags |= DM;
				break;
			case 'h':
			default:
				usage(g_path_get_basename(argv[0]));
				break;
		}
	}

	if(filterflags==0){
		filterflags=HD|SD;
	}
	

	while(sections[i].path!=NULL){
		if(sections[i].flags & filterflags){
			get_entities(&dev_list,&(sections[i]),displayflags);
		}
		i++;
	}
	
	if(scanflag!=0){
		//fd=inotify_init1(IN_NONBLOCK);
		fd=inotify_init();
		if(fd == -1){
			fprintf(stderr,"not scanning: unable to setup inotify\n");
			scanflag=0;
			goto endscan;
		}
		wd=inotify_add_watch(fd,"/dev",IN_CREATE);
		//wd=inotify_add_watch(fd,"/sys/block",IN_CREATE|IN_ONLYDIR);
		if(wd == -1){
			fprintf(stderr,"not scanning: unable to setup watch\n");
			scanflag=0;
			goto endscan;

		}
		fcntl(fd,F_SETFL,O_NONBLOCK);

		//write_string("/sys/class/scsi_host/","host","scan","- - -");
		write_string("/sys/class/fc_host/","host","issue_lip","1");

		tend=time(NULL)+30;
		while(time(NULL)<tend){
			sleep(1);
			write(1,".",1);
			len=read(fd,buf,1024);
			//printf("len=%d %d\n",len,errno);
			//if(len>0){
			//	printf("%d wd=%d mask=%x cookie=%u len=%d\n",len,event->wd,event->mask,event->cookie,event->len);
			//}
			if((len<0)&&(errno!=EAGAIN)){
				fprintf(stderr,"not scanning: failed to read from watch! %d\n",len);
				perror("read");
				scanflag=0;
				goto endscan;
			}
			j=0;
			while(j<len){
				event=&buf[j];
				k=0;
				while(sections[k].path!=NULL){
		        	        if((sections[k].flags & filterflags)
						&&(strncmp(event->name,sections[k].filter,strlen(sections[k].filter))==0)){
						//printf("added %s %s\n",event->name,sections[k].filter);
						create_entity(&dev_list,"/sys/block/",event->name,&(sections[k]),displayflags,"(added)");
						
					}
					k++;
				}
				j += sizeof(struct inotify_event) + event->len;
			}
		}
                printf("\n");
		inotify_rm_watch(fd,wd);
		close(fd);	
	}
	endscan:

	dev_list=g_slist_sort(dev_list,sortfunc);

	i=0;
	while(sections[i].path!=NULL){
		if((sections[i].flags & HEAD)&&(displayflags & HEAD)){
			print_headers(&(sections[i]),format,displayflags);
		}
	
		if(dev_list!=NULL){
			do{
		                //printf("flags= %d\n",displayflags);

				print_entities((GHashTable *) dev_list->data,&(sections[i]),format,displayflags);
			}while((dev_list=g_slist_next(dev_list))!=NULL);
		}	
		i++;
	}

	return EXIT_SUCCESS;
}
