/*
** file_op.h 2010-08-19 xueyingfei
**
** Copyright flying/xueyingfei.
**
** This file is part of MaxTable.
**
** Licensed under the Apache License, Version 2.0
** (the "License"); you may not use this file except in compliance with
** the License. You may obtain a copy of the License at
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
** implied. See the License for the specific language governing
** permissions and limitations under the License.
*/


#ifndef FILE_OP_H_
#define FILE_OP_H_


extern char	Kfsserver[32];
extern int	Kfsport;


//#include "mt2kfs.h"
//#include "dlfcn.h"
extern int
kfs_open(char *fname, int flag, char *serverHost, int port);

extern int
kfs_create(char *fname, char *serverHost, int port);

extern int
kfs_read(int fd,char *buf, int len, char *serverHost, int port);

extern int
kfs_mkdir(char *tab_dir, char *serverHost, int port);

extern int
kfs_write(int fd, char *buf, int buf_len, char *serverHost, int port);

extern int
kfs_close(int fd, char *serverHost, int port);

extern int
kfs_seek(int fd, int offset, char *serverHost, int port);

extern int
kfs_exist(char *tab_dir, char *serverHost, int port);


#define	OPEN(fd, tab_dir, flag)							\
	do{									\
		fd = kfs_open((char *)(tab_dir), (flag | O_CREAT), Kfsserver, Kfsport);		\
										\
		if (fd < 0)							\
		{								\
			fprintf(stderr, "open file failed for %s\n", strerror(errno));\
		}								\
										\
	}while(0)

#define	MKDIR(status, tab_dir, flag)						\
	do{									\
		status = kfs_mkdir((char *)(tab_dir), Kfsserver, Kfsport); 					\
										\
		if (status < 0)							\
		{								\
			fprintf(stderr, "mkdir failed for %s\n", strerror(errno));\
		}								\
	}while(0)

#define	READ(fd, buf, len)	kfs_read((fd), (char *)(buf), (len), Kfsserver, Kfsport)
	
#define	WRITE(fd, buf, buf_len)	kfs_write((fd), (char *)(buf), (buf_len), Kfsserver, Kfsport)

#define	CLOSE(fd)		kfs_close(fd, Kfsserver, Kfsport)

#define LSEEK(fd, offset)	kfs_seek(fd, offset, Kfsserver, Kfsport)

#define	STAT(dir, state)	stat((dir), (state))

#define EXIST(dir)		kfs_exist((char *)(dir), Kfsserver, Kfsport)


int
file_read(char *file_path, char *buf, int file_size);

void 
file_crt_or_rewrite(char *file_name, char* content);

int
file_exist(char* file_path);

int
file_get_size(char *file_path);

#endif 