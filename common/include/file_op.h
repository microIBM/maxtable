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


#define	OPEN(fd, tab_dir, flag)							\
	do{									\
		fd = open((tab_dir), (flag | O_CREAT), 0666);			\
										\
		if (fd < 0)							\
		{								\
			fprintf(stderr, "open file failed for %s\n", strerror(errno));\
		}								\
										\
	}while(0)

#define	MKDIR(status, tab_dir, flag)						\
	do{									\
		status = mkdir((tab_dir), (flag)); 				\
										\
		if (status < 0)							\
		{								\
			fprintf(stderr, "mkdir failed for %s\n", strerror(errno));\
		}								\
	}while(0)

#define	READ(fd, buf, len)	read((fd), (buf), (len))
	
#define	WRITE(fd, buf, buf_len)	write((fd), (buf), (buf_len))

#define	CLOSE(fd)		close(fd)

#define LSEEK(fd, offset, flag)	lseek(fd, offset, flag)

#define	STAT(dir, state)	stat((dir), (state))


int
file_read(char *file_path, char *buf, int file_size);

void 
file_crt_or_rewrite(char *file_name, char* content);

int
file_exist(char* file_path);

int
file_get_size(char *file_path);

#endif 
