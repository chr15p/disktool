/*
* Copyright (C) 2010 Chris Procter
*
* This file is part of scsi_admin
*
* This copyrighted material is made available to anyone wishing to use,
* modify, copy, or redistribute it subject to the terms and conditions
* of the GNU General Public License v.2.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <dirent.h>
#include <libdevmapper.h>
#include <blkid/blkid.h>

#include "directory.h"

#define BUFFSIZE 4096

GHashTable * mpathdevs=NULL;
GHashTable * dm_devnames=NULL;
GHashTable * dm_devnos=NULL;

gint compare_addresses(gconstpointer a,gconstpointer b)
{
	char *astr;
	char *bstr;
	char *end_a;
	char *end_b;
	int a_part;
	int b_part;
	char *alen;
	char *blen;


	astr=g_hash_table_lookup((GHashTable *)a,"device");
	bstr=g_hash_table_lookup((GHashTable *)b,"device");

	if((astr==NULL)&&(bstr==NULL)){
		return 0;
	}else if((astr==NULL)&&(bstr!=NULL)){
		return -1;
	}else if((astr!=NULL)&&(bstr==NULL)){
		return 1;
	}

	alen=astr+strlen(astr);
	blen=bstr+strlen(bstr);

	while((astr<=alen)&&(bstr<=blen)){
		//printf("%s %s\n",astr,bstr);
		a_part=strtol(astr,&end_a,10);
		b_part=strtol(bstr,&end_b,10);
		//printf("%d %d\n",a_part,b_part);
		if(a_part>b_part){
			return 1;
		}else if(a_part<b_part){
			return -1;
		}
		astr=end_a+1;
		bstr=end_b+1;
	}

	return 0;
}




char * size_to_human(char * blockstr)
{
	int i;
	char *u="BKMGTP";
	char *out;
	unsigned long long blocks;

	blocks=strtoull(blockstr,NULL,10);
	out=(char*)calloc(10,sizeof(char));
	if(out==NULL){
		fprintf(stderr,"Out of memory calculating sizes\n");
		exit(1);
	}

	if(blocks == 0)
		return 0;

	blocks=blocks<<9;

	for(i=0;i<6;i++)
	{
		if(blocks>1024)
		{
			//blocks=blocks/1024;
			blocks=blocks>>10;
		}else{
			break;
		}
	}

	snprintf(out,10,"%.1llu%c",blocks,*(u+i));

	return out;
}

char *  last_element(char * path)
{
	char * value;
	int len;
	int i;

	len=strlen(path);
	if(len==0){
		return NULL;
	}
	
	for(i=len-2;i>=0;i--){
		if(*(path+i)=='/'){
			value=(char*)malloc(len-i);
			strcpy(value,(path+i+1));
			*(value+len-i-1)='\0';
			break;
		}
	}

	if(*(value+len-i-2)=='/'){
		*(value+len-i-2)='\0';
	};
	return value;
}



int get_dirname(GHashTable *hash,char * path,char * location,int flags)
{
	char * value;

	value=last_element(path);

	g_hash_table_insert(hash,location,value);
	return 0;
}


int linkvalue(GHashTable *hash,char * path,char * location,int flags)
{
	char *file;
	char *value;
	int len;

	file=(char*)malloc(strlen(path)+strlen(location)+1);
	strcpy(file,path);
	strcat(file,location);

	value=(char*) malloc(BUFFSIZE);
	len=readlink(file,value,BUFFSIZE);
	*(value+len)=0;

	g_hash_table_insert(hash,location,last_element(value));
	free(file);
	return 0;
}


char * file_contents(char * path,char * location)
{
	char * file;
	char * value;
	int attrfile;
	int i=1;

	//printf("location=%s\n",location);
	
	file=(char*)malloc(strlen(path)+strlen(location)+1);
	//file=(char*)calloc(1,strlen(path)+strlen(location)+1);
	strcpy(file,path);
	strcat(file,location);

	//printf("file=%s\n",file);
	attrfile=open(file,O_RDONLY);
	if(attrfile==-1){
		//fprintf(stderr,"unable to open %s\n",file);
		return NULL;
	}
	value=(char*) calloc(BUFFSIZE,sizeof(char));
	read(attrfile,value,BUFFSIZE);
	while((*(value+strlen(value)-i)==' ')||(*(value+strlen(value)-i)=='\n')){
		*(value+strlen(value)-1)=0;
	}
	//if(*(value+strlen(value)-1)=='\n'){
	//	*(value+strlen(value)-1)=0;
	//}
	//printf("location=%s=value=%s=\n",location,value);
	close(attrfile);
	free(file);

	return value;
}

int get_size(GHashTable *hash,char * path,char * location,int flags)
{
	char * blocks;
	char * value;

	blocks=file_contents(path,location);
	if(blocks!=NULL){
		value=size_to_human(blocks);
	}
	g_hash_table_insert(hash,location,value);

	return 0;
}

int get_scheduler(GHashTable *hash,char * path,char * location,int flags)
{
	char * blocks;
	int i=0;
	int start=-1;

	blocks=file_contents(path,location);
	if(blocks==NULL){
		return 0;
	}
	while((*(blocks+i) != ']')&&(*(blocks+i)!=0)){
		if(*(blocks+i)=='['){
			start=i;
		}
		i++;
	}
	*(blocks+i)=0;
	if(start != -1){
		g_hash_table_insert(hash,location,blocks+start+1);
	}else{
		g_hash_table_insert(hash,location,blocks);
	}

	return 0;
}


int get_file_contents(GHashTable *hash,char * path,char * location,int flags)
{
	g_hash_table_insert(hash,location,file_contents(path,location));

	return 0;
}


int delete(GHashTable *hash,char * path,char * location,int flags)
{
	char * dev;
	int fd;
	char * deletefile;
	int delete_fd;
	char buf[512];

	//printf("delete\n");
	dev=(char*)malloc(15);
	strcpy(dev,"/dev/");
	strcat(dev, (char*)g_hash_table_lookup(hash,"hostname"));

	fd=open(dev,O_RDONLY);
	if(fd < 0)
	{
		//if we can't open it then it probably doesn't exist
		//but we'll claim its ok just to be safe
		free(dev);
		return 0;
	}

	free(dev);


	if(read(fd,buf,sizeof(buf)) < 0)
	{
		//if the device node exists but can't be read from then it doesn't exist
 
		deletefile=(char*)malloc(strlen(path)+strlen(location)+1);
		strcpy(deletefile,path);
		strcat(deletefile,location);

		delete_fd=open(deletefile,O_WRONLY);
		if(delete_fd==-1){
			fprintf(stderr,"unable to open %s\n",deletefile);
			return 1;
		}
		write(delete_fd,"1",strlen("1"));
		close(delete_fd);
		syslog(LOG_INFO,"deleted %s",(char *)g_hash_table_lookup(hash,"device"));
		g_hash_table_insert(hash,"note","(deleted)");
		free(deletefile);
	}

	return 0;
}


int rescan(GHashTable *hash,char * path,char * location,int flags)
{
	char * rescanfile;
	int rescan_fd;
	char * oldsize;
	char * size;
	char * blocks;

	rescanfile=(char*)malloc(strlen(path)+strlen(location)+1);
	strcpy(rescanfile,path);
	strcat(rescanfile,location);
	rescan_fd=open(rescanfile,O_WRONLY);
	if(rescan_fd==-1){
		fprintf(stderr,"unable to open %s\n",rescanfile);
		free(rescanfile);
		return 1;
	}
	write(rescan_fd,"1",strlen("1"));
	close(rescan_fd);

	oldsize=g_hash_table_lookup(hash,"size");

	blocks=file_contents(path,location);
	if(blocks!=NULL){
		size=size_to_human(blocks);
	
		if(strcmp(size,oldsize)!=0){
			syslog(LOG_INFO,"resized %s",(char *)g_hash_table_lookup( hash,"device"));
			g_hash_table_replace(hash,"size",size);
			g_hash_table_insert(hash,"note","(resized)");
		}
	}

	free(rescanfile);

	return 0;
}



int get_mpathdev2(GHashTable *hash,char * path,char * location,int flags)
{

	DIR *dir;
	struct dirent* ent;
	char * devfile;
	struct dm_task *dmt=0;
	struct dm_deps *deps=0;
	int i;
	struct stat* statbuf;
	gpointer dmdev;
	char * device;

	if(mpathdevs==NULL){
		if(!(dir=opendir("/dev/mapper"))){
			fprintf(stderr,"unable to open %s\n","/dev/mapper");
			exit(2);
		}
		mpathdevs = g_hash_table_new(&g_str_hash,&g_str_equal);
		dm_devnos = g_hash_table_new(&g_str_hash,&g_str_equal);
		while((ent=readdir(dir)) != NULL){
			if(strncmp(ent->d_name,"mpath",5)==0){
	
				devfile=(char*)malloc(sizeof(ent->d_name)+13);
				strcpy(devfile,"/dev/mapper/");
				strcat(devfile,ent->d_name);

				statbuf=(struct stat*) malloc(sizeof(struct stat));
				if(stat(devfile,statbuf)!=0){
					printf("stat error!\n");	
				}

				g_hash_table_insert(dm_devnames,ent->d_name,blkid_devno_to_devname(statbuf->st_rdev));
				g_hash_table_insert(dm_devnos,ent->d_name,&(statbuf->st_rdev));

				dmt = dm_task_create(DM_DEVICE_DEPS);
				if(!dmt){
					printf("create error!\n");
					exit(1);
				}
	
				dm_task_set_major(dmt,major(statbuf->st_rdev));
				dm_task_set_minor(dmt,minor(statbuf->st_rdev));
				if(!dm_task_run(dmt)){
					printf("run error!\n");
					exit(1);
				}

				deps=dm_task_get_deps(dmt);
				if(deps==NULL){
					printf("deps error!\n");
					exit(1);
				}
	
				for (i = 0; i < deps->count; i++){
					device=last_element(blkid_devno_to_devname(deps->device[i]));
					g_hash_table_insert(mpathdevs,device,ent->d_name);
				}
				free(statbuf);
				free(devfile);
			}
		}
	}

	dmdev=g_hash_table_lookup(hash,"hostname");
			
	g_hash_table_insert(hash,location,g_hash_table_lookup(mpathdevs,dmdev));
	
	return 0;
}


int get_dmdev2(GHashTable *hash,char * path,char * location,int flags)
{
	char * dmdev;

        dmdev=g_hash_table_lookup(hash,"mpathdev");
	if(dmdev!=NULL){
		g_hash_table_insert(hash,location,g_hash_table_lookup(dm_devnames,dmdev));
	}

	return 0;
}

int get_dmdevno(GHashTable *hash,char * path,char * location,int flags)
{
	dev_t *dmdev;
	char * val;

        dmdev=g_hash_table_lookup(hash,"mpathdev");
	if(dmdev!=NULL){
		val=(char*)malloc(10*sizeof(char));
		snprintf(val,10*sizeof(char),"%d:%d",major(*dmdev),minor(*dmdev));
		g_hash_table_insert(hash,location,val);
	}

	return 0;
}

//************************************
// All functions after this is infrastructure (rather then callbacks)
//************************************
//void print_entities(GHashTable* hash,Column cols[],char * format,int displayflags)
void print_line(GHashTable* hash,char * heading,char * loc,char * format,int flags)
{
	char * value=NULL;

	//printf("loc=%s\n",loc);
	value=g_hash_table_lookup(hash,loc);
       	//printf("print_line value=%s=\n",value); 
	if(value!=NULL){
		printf("%s: %s\n",heading,value); 
	}else{
		printf("%s:\n",heading); 
	}
	return;
}

void print_field(GHashTable* hash,char * heading,char * loc,char * format,int flags)
{
	char * value=NULL;

	value=g_hash_table_lookup(hash,loc);
       	//printf("print loc=%s=value=%s=\n",loc,value); 
	if(value != NULL){
		printf(format,value); 
	}else{
		printf(format,""); 
	}
	return;
}

void print_entities(GHashTable* hash,Filesearch search[],char * format,int displayflags)
{
	int i=0;

	while((search->cols)[i].flags != 0){
		//printf(" %d=%d=%d ",(search->cols)[i].flags,displayflags,((search->cols)[i].flags & displayflags));
		if(((search->cols)[i].printfunc!=NULL)		\
			&& (((search->cols)[i].flags & displayflags)!=0)){
	       		((search->cols)[i].printfunc)(hash,(search->cols)[i].heading,(search->cols)[i].location,format,search->flags);
		}
		i++;
	}

	printf("\n");
}

void print_headers(Filesearch search[],char * format,int displayflags)
{
	int i=0;

	while((search->cols)[i].flags != 0){
		if(((search->cols)[i].printfunc != NULL)		\
				&&((search->cols)[i].heading!=NULL)	\
				&&((search->cols)[i].flags & displayflags)){
			printf(format,(search->cols)[i].heading);
		}
		i++;
	}

	printf("\n");
}


void create_entity(GSList ** devlist,char *path,char *name,Filesearch search[],int displayflags,char * note)
{
	char * fullpath;
	GHashTable * hash;
	int j=0;

	fullpath=(char*)malloc(strlen(path)+strlen(name)+2);
	strcpy(fullpath,path);
	strcat(fullpath,name);
	strcat(fullpath,"/");

	hash=g_hash_table_new(&g_str_hash,&g_str_equal);
	while((search->cols)[j].flags != 0){
		if(((search->cols)[j].func != NULL)&&((search->cols)[j].flags & displayflags ) ){
			((search->cols)[j].func)(hash,fullpath,(search->cols)[j].location,search->flags);
		}
		j++;
	}
	if(note!=NULL){
		g_hash_table_insert(hash,"note",note);
	}

	if((hash!=NULL)&&(hash!=NULL)){
		(*devlist)=g_slist_prepend(*devlist,hash);
	}

	free(fullpath);
	return;
}


int get_entities(GSList ** devlist,Filesearch search[],int displayflags)
{
	DIR *dirstream;
	struct dirent* entry;

	dirstream=opendir(search->path);
	if(dirstream==NULL){
		fprintf(stderr,"unable to open %s\n",search->path);
		exit(1);
	}
	
	while((entry=readdir(dirstream))!=NULL){
		if(strncmp(entry->d_name,search->filter,strlen(search->filter))==0){
			create_entity(devlist,search->path, entry->d_name,search,displayflags,NULL);
		}
	}
	closedir(dirstream);
	return 0;
}


int write_string(char* path,char * filter,char * location,char * string)
{
	DIR *dirstream;
	struct dirent* entry;
	int msglen;
	int pathlen;
	char * fullpath;
	int propfile;
	

	dirstream=opendir(path);
	if(dirstream==NULL){
		fprintf(stderr,"unable to open %s\n",path);
		exit(1);
	}
	

	msglen=strlen(string);
	pathlen=strlen(path)+strlen(location)+3;

	while((entry=readdir(dirstream))!=NULL){
		if(strncmp(entry->d_name,filter,strlen(filter))==0){
			//fullpath=(char*)malloc(strlen(location)+pathlen);
			fullpath=(char*)malloc(strlen(entry->d_name)+pathlen);
			strcpy(fullpath,path);
			strcat(fullpath,"/");
			strcat(fullpath,entry->d_name);
			strcat(fullpath,"/");
			strcat(fullpath,location);

			//printf("fullpath=%s %s\n",fullpath,filter);
			//printf("Scanning %s\n",fullpath);
			//syslog(LOG_INFO,"Scanning %s",scan_file);
			propfile=open(fullpath,O_WRONLY);
			if(propfile==-1){
				fprintf(stderr,"Write_string: unable to open %s\n",fullpath);
				return 1;
			}
			write(propfile,string,msglen);
			close(propfile);
			//syslog(LOG_INFO,"Scanned %s\n",scan_file);
			free(fullpath);
		}
	}

	closedir(dirstream);
	return 0;
}

