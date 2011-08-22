/*
** cli.c 2010-06-25 xueyingfei
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
#include "global.h"
#include "utils.h"
#include "master/metaserver.h"
#include "region/rangeserver.h"
#include "netconn.h"
#include "conf.h"
#include "token.h"
#include "tss.h"
#include "parser.h"
#include "memcom.h"
#include "strings.h"
#include "trace.h"


extern	TSS	*Tss;

#define CONF_META_IP    "metaserver"
#define CONF_META_PORT  "metaport"

#define CLI_CONN_MASTER_MAX_LEN	64
#define CLI_CONN_REGION_MAX_LEN 64
#define CLI_CONF_PATH_MAX_LEN   64

/** cli related constants **/
#define CLI_CMD_CRT_TABLE   "create table"
#define CLI_CMD_QUIT "quit"

/* MaxTable */
#define CLI_DEFAULT_PREFIX "MTCli:"


typedef struct cli_infor
{
	/* default place is conf/cli.conf */
	char	cli_conf_path[CLI_CONF_PATH_MAX_LEN]; 
	char	cli_meta_ip[CLI_CONN_MASTER_MAX_LEN];
	int     cli_meta_port;
	char    cli_region_ip[CLI_CONN_REGION_MAX_LEN];
	int     cli_region_port;
	int     cli_status;
	//List *	cli_tab_infor;
} CLI_INFOR;

/* cli_status */
#define CLI_CONN_MASTER         0x0001
#define CLI_CONN_REGION         0x0002
#define CLI_CONN_OVER           0x0004
#define CLI_CONN_MASTER_AGAIN	0x0008

#define CLI_IS_CONN2MASTER(cli) (cli->cli_status & CLI_CONN_MASTER)
#define CLI_IS_CONN2REGION(cli) (cli->cli_status & CLI_CONN_REGION)
#define CLI_IS_CONN_OVER(cli)   (cli->cli_status & CLI_CONN_OVER)
#define CLI_IS_CONN2MASTER_AGAIN(cli)	(cli->cli_status & CLI_CONN_MASTER_AGAIN)

CLI_INFOR * Cli_infor = NULL;

void
cli_infor_init(char* conf_path)
{
	Cli_infor = MEMALLOCHEAP(sizeof(CLI_INFOR));

	/* Get thet path of configure. */
	MEMCPY(Cli_infor->cli_conf_path, conf_path, sizeof(conf_path));

	/* Get the IP and Port of metaserver. */
	conf_get_value_by_key(Cli_infor->cli_meta_ip, conf_path, CONF_META_IP);
	conf_get_value_by_key((char *)(&(Cli_infor->cli_meta_port)), conf_path, 
			      CONF_META_PORT);
	Cli_infor->cli_meta_port = atoi((char *)&(Cli_infor->cli_meta_port));

	/* Init the status of client. */
	Cli_infor->cli_status = CLI_CONN_MASTER;


	/* Load Metadata infor form Master */
}

static char *
cli_cmd_normalize(char *cmd)
{
	cmd = trim(cmd, LINE_SEPARATOR);
	cmd = trim(cmd, ' ');
	return cmd;
}


int
cli_deamon()
{
	LOCALTSS(tss);
	int	ret = 0;
	char 	*cli_str;
	char 	*ip;
	int	port;
	RPCRESP	*resp;
	RPCREQ 	*req;
	int 	sockfd;
	int 	querytype;
	int	meta_only;
	int 	meta_again;
	char	*send_rg_bp = NULL;
	int	send_rg_bp_idx;
	int	send_buf_size;
	char 	buf[LINE_BUF_SIZE];
	char	send_buf[LINE_BUF_SIZE];
	INSMETA	*resp_ins;
	char	tab_name[64];

	printf("Please type \"help\" if you need some further information.\n");
	
	while (1)
	{
		req = NULL;
		resp = NULL;
		meta_only = TRUE;
		send_buf_size = 0;
		
		printf(CLI_DEFAULT_PREFIX);
		
		cli_str = fgets(buf, LINE_BUF_SIZE, stdin);

		if (!cli_str)
		{
		    ret = ferror(stdin);
		    fprintf(stderr, "get cmd error %d\n", ret);
		    break; /* continue or break */
		}

		cli_str = cli_cmd_normalize(cli_str);

		send_buf_size = STRLEN(cli_str);
		if (send_buf_size == 0)
		{
		    continue;
		}

		parser_open(cli_str);

		querytype = ((TREE *)(tss->tcmd_parser))->sym.command.querytype;
		MEMSET(tab_name, 64);
		MEMCPY(tab_name, ((TREE *)(tss->tcmd_parser))->sym.command.tabname,
			((TREE *)(tss->tcmd_parser))->sym.command.tabname_len);

		/* Each command must send the request to metadata server first. */
		Cli_infor->cli_status = CLI_CONN_MASTER;

conn_again:
		if (CLI_IS_CONN2MASTER(Cli_infor) || CLI_IS_CONN2MASTER_AGAIN(Cli_infor))
		{
		    ip		= Cli_infor->cli_meta_ip;
		    port	= Cli_infor->cli_meta_port;
		}
		else if(CLI_IS_CONN2REGION(Cli_infor))
		{
		    ip		= Cli_infor->cli_region_ip;
		    port	= Cli_infor->cli_region_port;
		}
		   
		sockfd = conn_open(ip, port);

		MEMSET(send_buf, LINE_BUF_SIZE);
		MEMCPY(send_buf, RPC_REQUEST_MAGIC, RPC_MAGIC_MAX_LEN);
		/* Set the information header with the MAGIC. */
		MEMCPY((send_buf + RPC_MAGIC_MAX_LEN), cli_str, send_buf_size);

		//if ((Cli_infor->cli_status == CLI_CONN_REGION) && (meta_only == FALSE) && send_rg_bp)
		if (send_rg_bp)
		{
			MEMFREEHEAP(send_rg_bp);
			send_rg_bp = NULL;
		}
			
		write(sockfd, send_buf, (send_buf_size + RPC_MAGIC_MAX_LEN));

		resp = conn_recv_resp(sockfd);

		if (resp->status_code != RPC_SUCCESS)
		{
			printf("\n ERROR \n");
			goto finish;
		}

		meta_again = FALSE;
		
		switch(querytype)
		{
		    case ADDSERVER:
			meta_only = TRUE;
			break;
				
		    case TABCREAT:
			meta_only = TRUE;
			break;
			
		    case INSERT:
		
			if (CLI_IS_CONN2MASTER(Cli_infor))
			{
				resp_ins = (INSMETA *)resp->result;

				MEMCPY(Cli_infor->cli_region_ip, 
				       resp_ins->i_hdr.rg_info.rg_addr, 
				       CLI_CONN_REGION_MAX_LEN);

				Cli_infor->cli_region_port = 
					resp_ins->i_hdr.rg_info.rg_port;

				/* Override the UNION part for this reques. */
				MEMCPY(resp->result, RPC_REQUEST_MAGIC, RPC_MAGIC_MAX_LEN);

				send_buf_size = resp->result_length + STRLEN(cli_str);
				send_rg_bp = MEMALLOCHEAP(send_buf_size);				

				send_rg_bp_idx = 0;
				PUT_TO_BUFFER(send_rg_bp, send_rg_bp_idx, 
					      resp->result, resp->result_length);
				PUT_TO_BUFFER(send_rg_bp, send_rg_bp_idx, 
					      cli_str, STRLEN(cli_str));

				cli_str = send_rg_bp;

				meta_only = FALSE;
			}
			else if (!meta_again)
			{
				meta_only = TRUE;

				if (resp->result_length)
				{
					/*Split case. */

					
					char *cli_add_sstab = "addsstab into ";				
					int	sstab_id;

					send_buf_size = resp->result_length + 64 + STRLEN(cli_add_sstab);
					send_rg_bp = MEMALLOCHEAP(send_buf_size);				

					MEMSET(send_rg_bp, send_buf_size);

					char newsstabname[SSTABLE_NAME_MAX_LEN];

					MEMSET(newsstabname, SSTABLE_NAME_MAX_LEN);

					MEMCPY(newsstabname, resp->result, SSTABLE_NAME_MAX_LEN);

					sstab_id = *(int *)(resp->result + SSTABLE_NAME_MAX_LEN);

					sprintf(send_rg_bp, "addsstab into %s (%s, %d, %s)", tab_name, newsstabname,
						sstab_id, resp->result + SSTABLE_NAME_MAX_LEN + sizeof(int));

					cli_str = send_rg_bp;

					meta_again = TRUE;
				}
			}

			
			break;
			
		    case CRTINDEX:
			break;
			
		    case SELECT:
			if (CLI_IS_CONN2MASTER(Cli_infor))
			{
				resp_ins = (INSMETA *)resp->result;

				MEMCPY(Cli_infor->cli_region_ip, 
				       resp_ins->i_hdr.rg_info.rg_addr, 
				       CLI_CONN_REGION_MAX_LEN);

				Cli_infor->cli_region_port = 
					resp_ins->i_hdr.rg_info.rg_port;

				/* Override the UNION part for this reques. */
				MEMCPY(resp->result, RPC_REQUEST_MAGIC, RPC_MAGIC_MAX_LEN);

				send_buf_size = resp->result_length + STRLEN(cli_str);
				send_rg_bp = MEMALLOCHEAP(send_buf_size);				

				send_rg_bp_idx = 0;
				PUT_TO_BUFFER(send_rg_bp, send_rg_bp_idx, 
					      resp->result, resp->result_length);
				PUT_TO_BUFFER(send_rg_bp, send_rg_bp_idx, 
					      cli_str, STRLEN(cli_str));

				cli_str = send_rg_bp;

				meta_only = FALSE;
			}
			else
			{
				printf("Result : %s\n",resp->result);
				meta_only = TRUE;
			}
			break;
			
		    case DELETE:
			break;
			
		    default:
			break;
		}


		if (!meta_only)
		{		
			/* 
			** Connection step 1: connect to metadaserver.
			** Connection step 2: connect to region server.
			*/
			Cli_infor->cli_status = CLI_CONN_REGION;

			conn_close(sockfd, req, resp);

			goto conn_again;

		}

		if (meta_again)
		{
			Cli_infor->cli_status = CLI_CONN_MASTER_AGAIN;

			conn_close(sockfd, req, resp);

			goto conn_again;
			
		}

finish:		
		conn_close(sockfd, req, resp);
		parser_close();
		tss_init(tss);
	}
	
	return ret;
}




static int
cli_test_main(char *cmd)
{
	LOCALTSS(tss);
	int	ret = 0;
	char 	*cli_str;
	char 	*ip;
	int	port;
	RPCRESP	*resp;
	RPCREQ 	*req;
	int 	sockfd;
	int 	querytype;
	int	meta_only;
	int 	meta_again;
	char	*send_rg_bp = NULL;
	int	send_rg_bp_idx;
	int	send_buf_size;
	char	send_buf[LINE_BUF_SIZE];
	INSMETA	*resp_ins;
	char	tab_name[64];

	

	req = NULL;
	resp = NULL;
	meta_only = TRUE;
	send_buf_size = 0;
	
	//printf(CLI_DEFAULT_PREFIX);
	
	cli_str = cmd;

	if (!cli_str)
	{
	    ret = ferror(stdin);
	    fprintf(stderr, "get cmd error %d\n", ret);
	    return -1; 
	}

	cli_str = cli_cmd_normalize(cli_str);

	send_buf_size = STRLEN(cli_str);
	if (send_buf_size == 0)
	{
	    return -1;
	}

	parser_open(cli_str);

	querytype = ((TREE *)(tss->tcmd_parser))->sym.command.querytype;
	MEMSET(tab_name, 64);
	MEMCPY(tab_name, ((TREE *)(tss->tcmd_parser))->sym.command.tabname,
		((TREE *)(tss->tcmd_parser))->sym.command.tabname_len);

	/* Each command must send the request to metadata server first. */
	Cli_infor->cli_status = CLI_CONN_MASTER;

conn_again:
	if (CLI_IS_CONN2MASTER(Cli_infor) || CLI_IS_CONN2MASTER_AGAIN(Cli_infor))
	{
	    ip		= Cli_infor->cli_meta_ip;
	    port	= Cli_infor->cli_meta_port;
	}
	else if(CLI_IS_CONN2REGION(Cli_infor))
	{
	    ip		= Cli_infor->cli_region_ip;
	    port	= Cli_infor->cli_region_port;
	}
	   
	sockfd = conn_open(ip, port);

	MEMSET(send_buf, LINE_BUF_SIZE);
	MEMCPY(send_buf, RPC_REQUEST_MAGIC, RPC_MAGIC_MAX_LEN);
	/* Set the information header with the MAGIC. */
	MEMCPY((send_buf + RPC_MAGIC_MAX_LEN), cli_str, send_buf_size);

	//if ((Cli_infor->cli_status == CLI_CONN_REGION) && (meta_only == FALSE) && send_rg_bp)
	if (send_rg_bp)
	{
		MEMFREEHEAP(send_rg_bp);
		send_rg_bp = NULL;
	}
		
	write(sockfd, send_buf, (send_buf_size + RPC_MAGIC_MAX_LEN));

	resp = conn_recv_resp(sockfd);

	if (resp->status_code != RPC_SUCCESS)
	{
		printf("\n ERROR \n");
		goto finish;
	}

	meta_again = FALSE;

	switch(querytype)
	{
	    case ADDSERVER:
		meta_only = TRUE;
		break;
			
	    case TABCREAT:
		meta_only = TRUE;
		break;
		
	    case INSERT:
		
		if (CLI_IS_CONN2MASTER(Cli_infor))
		{
			resp_ins = (INSMETA *)resp->result;

			MEMCPY(Cli_infor->cli_region_ip, 
			       resp_ins->i_hdr.rg_info.rg_addr, 
			       CLI_CONN_REGION_MAX_LEN);

			Cli_infor->cli_region_port = 
				resp_ins->i_hdr.rg_info.rg_port;

			/* Override the UNION part for this reques. */
			MEMCPY(resp->result, RPC_REQUEST_MAGIC, RPC_MAGIC_MAX_LEN);

			send_buf_size = resp->result_length + STRLEN(cli_str);
			send_rg_bp = MEMALLOCHEAP(send_buf_size);				

			send_rg_bp_idx = 0;
			PUT_TO_BUFFER(send_rg_bp, send_rg_bp_idx, 
				      resp->result, resp->result_length);
			PUT_TO_BUFFER(send_rg_bp, send_rg_bp_idx, 
				      cli_str, STRLEN(cli_str));

			cli_str = send_rg_bp;

			meta_only = FALSE;
		}
		else if (!meta_again)
		{
			meta_only = TRUE;

			if (resp->result_length)
			{
				/*Split case. */

				
				char *cli_add_sstab = "addsstab into ";				
				int	sstab_id;
				
				send_buf_size = resp->result_length + 128 + STRLEN(cli_add_sstab);
				send_rg_bp = MEMALLOCHEAP(send_buf_size);				

				MEMSET(send_rg_bp, send_buf_size);

				char newsstabname[SSTABLE_NAME_MAX_LEN];

				MEMSET(newsstabname, SSTABLE_NAME_MAX_LEN);

				MEMCPY(newsstabname, resp->result, SSTABLE_NAME_MAX_LEN);
				
				sstab_id = *(int *)(resp->result + SSTABLE_NAME_MAX_LEN);
				sprintf(send_rg_bp, "addsstab into %s (%s, %d, %s)", tab_name, newsstabname,
						sstab_id, resp->result + SSTABLE_NAME_MAX_LEN + sizeof(int));
				
				cli_str = send_rg_bp;

				
				meta_again = TRUE;
			}
		}

		
		break;
		
	    case CRTINDEX:
		break;
		
	    case SELECT:
		if (CLI_IS_CONN2MASTER(Cli_infor))
		{
			resp_ins = (INSMETA *)resp->result;

			MEMCPY(Cli_infor->cli_region_ip, 
			       resp_ins->i_hdr.rg_info.rg_addr, 
			       CLI_CONN_REGION_MAX_LEN);

			Cli_infor->cli_region_port = 
				resp_ins->i_hdr.rg_info.rg_port;

			/* Override the UNION part for this reques. */
			MEMCPY(resp->result, RPC_REQUEST_MAGIC, RPC_MAGIC_MAX_LEN);

			send_buf_size = resp->result_length + STRLEN(cli_str);
			send_rg_bp = MEMALLOCHEAP(send_buf_size);				

			send_rg_bp_idx = 0;
			PUT_TO_BUFFER(send_rg_bp, send_rg_bp_idx, 
				      resp->result, resp->result_length);
			PUT_TO_BUFFER(send_rg_bp, send_rg_bp_idx, 
				      cli_str, STRLEN(cli_str));

			cli_str = send_rg_bp;

			meta_only = FALSE;
		}
		else
		{
			printf("Result : %s\n",resp->result);
			meta_only = TRUE;
		}
		break;
		
	    case DELETE:
		break;
		
	    default:
		break;
	}


	if (!meta_only)
	{		
		/* 
		** Connection step 1: connect to metadaserver.
		** Connection step 2: connect to region server.
		*/
		Cli_infor->cli_status = CLI_CONN_REGION;

		conn_close(sockfd, req, resp);

		goto conn_again;

	}

	if (meta_again)
	{
		Cli_infor->cli_status = CLI_CONN_MASTER_AGAIN;

		conn_close(sockfd, req, resp);

		goto conn_again;
		
	}


finish:		
	conn_close(sockfd, req, resp);
	parser_close();
	tss_init(tss);

	return ret;
}


#ifdef MAXTABLE_UNIT_TEST
int main(int argc, char **argv)
{
	char		*crtab;
	char		*instab;
	char		*c1;
	char		*c2;
	char		*seltab;
	char		*cli_conf_path;	
	int		i;


	mem_init_alloc_regions();

	Trace = 0;
	
	cli_conf_path = CLI_DEFAULT_CONF_PATH;
	conf_get_path(argc, argv, &cli_conf_path);

	cli_infor_init(cli_conf_path);

	tss_setup(TSS_OP_CLIENT);
	
	c1 = (char *)MEMALLOCHEAP(32);	
	c2 = (char *)MEMALLOCHEAP(32);

	instab = (char *)MEMALLOCHEAP(128);	
	seltab = (char *)MEMALLOCHEAP(128);	
	
	/* 1st step: create table. */
	crtab = "create table yxue (filename varchar, servername varchar)";

	printf("CRATING TABLE yxue\n");
	printf("create table yxue (filename varchar, servername varchar)\n");
	cli_test_main(crtab);


	Trace = 0;

	printf("Begain to INSERT data into the table yxue\n");
	/* 2nd step: insert into table. */
	for(i = 0; i < 10000; i++)
	{
		MEMSET(c1, 32);
		sprintf(c1, "%s%d", "gggg", i);
		MEMSET(instab, 128);
		sprintf(instab,"insert into yxue (%s, bbbb%d)", c1, i);

		if((i > 99) && ((i % 100) == 0))
		{
			printf("inserting %d rows into yxue\n", i);
		}
		cli_test_main(instab);
	}
	Trace = 0;


	printf("Begain to SELECT data from the table yxue\n");
	/* 3rd step: select data from table. */

	for(i = 1535; i < 1560; i++)
	{
		MEMSET(c1, 32);
		sprintf(c1, "%s%d", "gggg", i);
		MEMSET(seltab, 128);
		sprintf(seltab,"select yxue (%s)", c1);

		cli_test_main(seltab);
	}

	
	for(i = 5535; i < 5560; i++)
	{
		MEMSET(c1, 32);
		sprintf(c1, "%s%d", "gggg", i);
		MEMSET(seltab, 128);
		sprintf(seltab,"select yxue (%s)", c1);

		cli_test_main(seltab);
	}

	return -1;
}

#else
int main(int argc, char **argv)
{
	int ret;
	char *cli_conf_path;	


	mem_init_alloc_regions();

	cli_conf_path = CLI_DEFAULT_CONF_PATH;
	conf_get_path(argc, argv, &cli_conf_path);

	cli_infor_init(cli_conf_path);

	tss_setup(TSS_OP_CLIENT);
		
	ret = cli_deamon();

	tss_release();
	
	return ret;
}
#endif
