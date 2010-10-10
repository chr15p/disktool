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

#include "directory.h"

#define BUFFSIZE 4096


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

char *  dirname(char * path)
{
	char * value;
	int len;
	int i;

	len=strlen(path);
	
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

	value=dirname(path);

	g_hash_table_insert(hash,location,value);
	return 0;
}


int linkvalue(GHashTable *hash,char * path,char * location,int flags)
{
	char *file;
	char *value;

	file=(char*)malloc(strlen(path)+strlen(location)+1);
	strcpy(file,path);
	strcat(file,location);

	value=(char*) malloc(BUFFSIZE);
	readlink(file,value,BUFFSIZE);

	g_hash_table_insert(hash,location,dirname(value));
	free(file);
	return 0;
}


char * file_contents(char * path,char * location)
{
	char * file;
	char * value;
	int attrfile;

	//printf("location=%s\n",location);
	file=(char*)malloc(strlen(path)+strlen(location)+1);
	strcpy(file,path);
	strcat(file,location);

	//printf("file=%s\n",file);
	attrfile=open(file,O_RDONLY);
	if(attrfile==-1){
		//fprintf(stderr,"unable to open %s\n",file);
		return NULL;
	}
	value=(char*) malloc(BUFFSIZE);
	read(attrfile,value,BUFFSIZE);
	if(*(value+strlen(value)-1)=='\n'){
		*(value+strlen(value)-1)=0;
	}
	//printf("location=%s value=%s\n",location,value);
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


int get_file_contents(GHashTable *hash,char * path,char * location,int flags)
{
	g_hash_table_insert(hash,location,file_contents(path,location));

	return 0;
}



void print_line(GHashTable* hash,char * heading,char * loc,char * format,int flags)
{
	char * value=NULL;

	//printf("loc=%s\n",loc);
	value=g_hash_table_lookup(hash,loc);
       	//printf("print_line value=%s\n",value); 
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
	if(value!=NULL){
		printf(format,value); 
	}
	return;
}

//************************************
// All functions after this is infrastructure (rather then callbacks)
//************************************
//void print_entities(GHashTable* hash,Column cols[],char * format,int displayflags)
void print_entities(GHashTable* hash,Filesearch search[],char * format)
{
	int i=0;

	while((search->cols)[i].location != NULL){
		if((search->cols)[i].printfunc!=NULL){
	       		((search->cols)[i].printfunc)(hash,(search->cols)[i].heading,(search->cols)[i].location,format,search->flags);
		}else{
			printf(format,""); 
		}
		i++;
	}

	printf("\n");
}

void print_headers(Filesearch search[],char * format)
{
	int i=0;

	while((search->cols)[i].location != NULL){
		if((search->cols)[i].heading!=NULL){
			printf(format,(search->cols)[i].heading);
		}else{
			printf(format,""); 
		}
		i++;
	}

	printf("\n");
}




int get_entities(GSList ** devlist,Filesearch search[])
{
	int j;
	GHashTable * hash;
	DIR *dirstream;
	struct dirent* entry;
	char * path;

	dirstream=opendir(search->path);
	if(dirstream==NULL){
		fprintf(stderr,"unable to open %s\n",search->path);
		exit(1);
	}
	
	while((entry=readdir(dirstream))!=NULL){
		j=0;
		//printf("j=%d %s %s %d\n",j,entry->d_name,search->filter,strlen(search->filter));
		if(strncmp(entry->d_name,search->filter,strlen(search->filter))==0){
			path=(char*)malloc(strlen(search->path)+strlen(entry->d_name)+2);
			strcpy(path,search->path);
			strcat(path,entry->d_name);
			strcat(path,"/");
			//printf("path=%s\n",path);

			
			hash=g_hash_table_new(&g_str_hash,&g_str_equal);
			while((search->cols)[j].location != NULL){
				if((search->cols)[j].func != NULL ){
					((search->cols)[j].func)(hash,path,(search->cols)[j].location,search->flags);
				}
				j++;
			}
			
			if((hash!=NULL)&&(hash!=NULL)){
	                        (*devlist)=g_slist_prepend(*devlist,hash);
	                }
		
			free(path);
		}
	}
	closedir(dirstream);
	return 0;
}

