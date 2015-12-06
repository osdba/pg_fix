/*
 * contrib/pagefix/pagefix.c
 *
 *
 * pagefix.c
 *
 *
 * Original author:		osdba(TangCheng)
 * Current maintainer:	osdba(TangCheng)
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "utils/builtins.h"


#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>

#include "pg_getopt.h"

const char *progname;

const char * report_bug_info = "Report bugs to osdba<chengdata@gmail.com>.";

int
clean_tuple_hint(char * heap_file, int page, int debug);

int
show_heap_file(char * heap_file, int page);

int
get_xid_status(char * commit_log_file, uint32 xid);

int
set_xid_status(char * commit_log_file, uint32 xid, unsigned char xid_status);

int
clean_tuple_hint(char * heap_file, int page, int debug)
{
	char        buf[BLCKSZ];
	int         fd;
	int         ret;
	int         cnt;
	int         i;
	int         blocks;
	ItemId		id;
	uint16		lp_offset;
	uint16		lp_flags;
	uint16		lp_len;
	HeapTupleHeader tuphdr;


	fd = open(heap_file, O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "Can not open %s", heap_file);
		perror("");
		return 1;
	}

    blocks = 0;
	while(1)
	{
		if (page != -1)
		{
		    ret = pread(fd, buf, BLCKSZ, BLCKSZ*page);
		}
		else
		{
		    ret = pread(fd, buf, BLCKSZ, BLCKSZ*blocks);
		}
		if (ret == 0)
		{
			break;
		}
		if (ret < 0)
		{
			perror("");
			break;
		}
		cnt = PageGetMaxOffsetNumber((Page)buf);
		for (i=0; i<cnt; i++)
		{
			id = PageGetItemId((Page)buf, i+1);
			lp_offset = ItemIdGetOffset(id);
			lp_flags = ItemIdGetFlags(id);
			lp_len = ItemIdGetLength(id);

			if (!(ItemIdHasStorage(id) &&
				lp_len >= sizeof(HeapTupleHeader) &&
				lp_offset == MAXALIGN(lp_offset) &&
				lp_offset + lp_len <= BLCKSZ))
			{
				continue;
			}

			tuphdr = (HeapTupleHeader) PageGetItem((Page)buf, id);
			tuphdr->t_infomask &= (~HEAP_XMIN_COMMITTED);
			tuphdr->t_infomask &= (~HEAP_XMIN_INVALID);
			tuphdr->t_infomask &= (~HEAP_XMAX_COMMITTED);
			tuphdr->t_infomask &= (~HEAP_XMAX_INVALID);
			ret = pwrite(fd, buf, 8192, 8192*blocks);
		}

		if (page != -1)
		{
			break;
		}

		blocks++;
	}
	close(fd);
	return 0;
}


int
show_heap_file(char * heap_file, int page)
{
	char        buf[BLCKSZ];
	int         fd;
	int         ret;
	int         cnt;
	int         i;
	int         blocks;
	ItemId		id;
	uint16		lp_offset;
	uint16		lp_flags;
	uint16		lp_len;
	HeapTupleHeader tuphdr;


	fd = open(heap_file, O_RDONLY);
	if (fd < 0)
	{
		fprintf(stderr, "Can not open %s", heap_file);
		perror("");
		return 1;
	}

	// "%4d %5d %5d %6d %10u %10u %10u %5u %5u %8x %9x %4u %10u\n
	printf(" lp   off  flags lp_len    xmin       xmax      field3   blkid posid infomask infomask2 hoff     oid\n");
	printf("---- ----- ----- ------ ---------- ---------- ---------- ----- ----- -------- --------- ----  ----------\n");

    blocks = 0;
	while(1)
	{
		if (page != -1)
		{
		    ret = pread(fd, buf, BLCKSZ, BLCKSZ*page);
		}
		else
		{
		    ret = pread(fd, buf, BLCKSZ, BLCKSZ*blocks);
		}
		if (ret == 0)
		{
			break;
		}
		if (ret < 0)
		{
			perror("");
			break;
		}
		cnt = PageGetMaxOffsetNumber((Page)buf);
		for (i=0; i<cnt; i++)
		{
			id = PageGetItemId((Page)buf, i+1);
			lp_offset = ItemIdGetOffset(id);
			lp_flags = ItemIdGetFlags(id);
			lp_len = ItemIdGetLength(id);
			if (!(ItemIdHasStorage(id) &&
				lp_len >= sizeof(HeapTupleHeader) &&
				lp_offset == MAXALIGN(lp_offset) &&
				lp_offset + lp_len <= BLCKSZ))
			{
				continue;
			}
			tuphdr = (HeapTupleHeader) PageGetItem((Page)buf, id);

			/* "lp  off  flags lp_len    xmin       xmax      field3   blkid posid infomask infomask2 hoff       oid"*/
			printf("%4d %5d %5d %6d %10u %10u %10u %5u %5u %8x %9x %4u %10u\n",
					i+1,
					lp_offset,
					lp_flags,
					lp_len,
					tuphdr->t_choice.t_heap.t_xmin,
					tuphdr->t_choice.t_heap.t_xmax,
					tuphdr->t_choice.t_heap.t_field3.t_cid,
					(uint32)tuphdr->t_ctid.ip_blkid.bi_hi*65536 + tuphdr->t_ctid.ip_blkid.bi_lo,
					tuphdr->t_ctid.ip_posid,
					tuphdr->t_infomask,
					tuphdr->t_infomask2,
					tuphdr->t_hoff,
					HeapTupleHeaderGetOid(tuphdr)
				   );
		}

		if (page != -1)
		{
			break;
		}

		blocks++;
	}
	close(fd);
	return 0;
}

static const char *
xid_status_to_str( unsigned char xid_status)
{
    /* 事务有四种状态
    #define TRANSACTION_STATUS_IN_PROGRESS      0x00
    #define TRANSACTION_STATUS_COMMITTED        0x01
    #define TRANSACTION_STATUS_ABORTED          0x02
    #define TRANSACTION_STATUS_SUB_COMMITTED    0x03
    */
    switch(xid_status)
    {
    case 0:
        return "IN_PROGRESS";
    case 1:
        return "COMMITTED";
    case 2:
        return "ABORTED";
    case 3:
        return "SUB_COMMITTED";
    default:
        return "INVALID";
    }
}

int
set_xid_status(char * commit_log_file, uint32 xid, unsigned char xid_status)
{
	char        buf[512];
	int         fd;
	int         ret;
	uint32      position;
	uint32      blocks;
	uint32      offset;
	uint32      bit_off;
	unsigned char tmp_ch1, tmp_ch2;
	unsigned char old_status;
    
	fd = open(commit_log_file, O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "Can not open %s", commit_log_file);
		perror("");
		return 1;
	}

	position = xid / 4;
	blocks = position / 512;
	offset = position % 512;
	bit_off = (xid % 4)*2;

	memset(buf, 0, 512);
	ret = pread(fd, buf, 512, blocks*512);
    if (ret < 0)
    {
	    fprintf(stderr, "Can not read file %s in offset %u", commit_log_file, blocks*512);
	    perror("");
	    return 1;
    }

    /* 下面把指定位置bit_off,bit_off+1的两位设置为指定的状态值xid_status，
     * 方法为，先生成tmp_ch1，tmp_ch1的值为除要设置的两位为真实的状态值外，其它bit都为1的，
     * 再生成tmp_ch2，tmp_ch2的值为除要设置的两bit位全为1外，其它bit为原先的值
     * 最后，把tmp_ch1与tmp_ch2做位与计算就所bit_off位置的两个bit值设置为了指定的状态了。
    */
    /*下面的操作之后，bit_off+1之前的位都置为了1t，bit_off-1后的位都为0，bit_off到bit_off+1的两位保存了原状态值*/
    tmp_ch1 = (xid_status | 0xFC) << bit_off; 
    /*(0xFF >>(8 - bit_off))的结果是bit_off之后的位都为1了*/
	tmp_ch1 = tmp_ch1 | (0xFF >>(8 - bit_off));

	/*把tmp_ch2中要设置的两位变为1外，其它不变*/
    tmp_ch2= buf[offset];
    tmp_ch2 |= (1 << bit_off | 1 << (bit_off + 1));

    old_status = buf[offset];
	old_status >>= bit_off;
    old_status &= 3;

	//printf("xid=%u, pre byte=%x, tmp_ch1=%x, tmp_ch2=%x\n", xid, buf[offset], tmp_ch1, tmp_ch2);
    buf[offset] = tmp_ch1 & tmp_ch2;
	//printf("xid=%u, byte=%x\n", xid, buf[offset]);

    ret = pwrite(fd, buf, 512, blocks*512);
    close(fd);
	printf("xid(%u) status from %x(%s) change to %x(%s)\n", 
            xid, 
            old_status, xid_status_to_str(old_status), 
            xid_status, xid_status_to_str(xid_status));
	return 0;
}

int
get_xid_status(char * commit_log_file, uint32 xid)
{
	char        buf[512];
	int         fd;
	int         ret;
	uint32      position;
	uint32      blocks;
	uint32      offset;
	uint32      bit_off;
	unsigned char tmp_ch;
    
	fd = open(commit_log_file, O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "Can not open %s", commit_log_file);
		perror("");
		return 1;
	}

	position = xid / 4;
	blocks = position / 512;
	offset = position % 512;
	bit_off = (xid % 4)*2;
	memset(buf, 0, 512);
	ret = pread(fd, buf, 512, blocks*512);
    if (ret < 0)
    {
	    fprintf(stderr, "Can not read file %s in offset %u", commit_log_file, blocks*512);
	    perror("");
	    return 1;
    }
    close(fd);

    tmp_ch = buf[offset];
	tmp_ch = tmp_ch >> bit_off;
    tmp_ch = tmp_ch & 3;
	printf("xid(%u) status is %u(%s)\n", xid, tmp_ch, xid_status_to_str(tmp_ch));
	return 0;
}



static void
help_show_tuple(void)
{
	printf("Usage:\n");
	printf("  %s show_tuple [OPTION]\n", progname);
	printf("\nOptions:\n");
	printf("  -f <relation file>\n"
		   "    specifies relation file\n");
	printf("  -p <page>\n"
		   "    Specifies the page of relation file\n");
	printf("  -h\n"
		   "    show this help, the exit\n");
	printf("\n%s\n", report_bug_info);
}

static int
cmd_show_tuple(int argc, char * argv[])
{
    int c;
	char        *heap_file = NULL;
	int         page = -1;

	while ((c = getopt(argc-1, &argv[1], "hf:p:")) != -1)
	{
		switch (c)
		{
            case 'h':
                help_show_tuple();
                return 0;
			case 'f':
				heap_file = optarg;
				break;
			case 'p':
				page = atoi(optarg);
				break;
			default:
				fprintf(stderr, 
                        "Try \"%s %s --help\" for more information.\n", 
                        progname, argv[1]);
				return 2;
				break;
		}
	}
    if (argc == 2)
	{
		fprintf(stderr, 
                "%s %s : not enough command-line arguments\n", 
                progname, argv[1]);
		return 2;
	}
    return show_heap_file(heap_file, page);
}

static void
help_clean_tuple_hint(void)
{
	printf("Usage:\n");
	printf("  %s clean_tuple_hint [OPTION]\n", progname);
	printf("\nOptions:\n");
	printf("  -f <relation file>\n"
		   "    specifies relation file\n");
	printf("  -d\n"
		   "     Output debug information\n");
	printf("  -p <page>\n"
		   "    Specifies the page of relation file\n");
	printf("  -h\n"
		   "    show this help, the exit\n");
	printf("\n%s\n", report_bug_info);
}

static int
cmd_clean_tuple_hint(int argc, char * argv[])
{
    int c;
	char        *heap_file = NULL;
	int         page = -1;
	int         debug = 0;

	while ((c = getopt(argc-1, &argv[1], "dhf:p:")) != -1)
	{
		switch (c)
		{
            case 'h':
                help_clean_tuple_hint();
                return 0;
            case 'd':
                debug = 1;
                break;
			case 'f':
				heap_file = optarg;
				break;
			case 'p':
				page = atoi(optarg);
				break;
			default:
				fprintf(stderr, 
                        "Try \"%s %s -h\" for more information.\n", 
                        progname, argv[1]);
				return 2;
				break;
		}
	}
    if (argc == 2)
	{
		fprintf(stderr, 
                "%s %s : not enough command-line arguments\n", 
                progname, argv[1]);
		return 2;
	}
    return clean_tuple_hint(heap_file, page, debug);
}

static void
help_set_xid_status(void)
{
	printf("Usage:\n");
	printf("  %s set_xid_status [OPTION]\n", progname);
	printf("\nOptions:\n");
	printf("  -f <commit_log_file>\n"
		   "    specify commit log file\n");
	printf("  -x <xid>\n"
		   "    Specifies xid that you want to change it's status\n");
	printf("  -s <xid_status>\n"
		   "    Specifies the status of xid you want to set\n");
	printf("  -h show this help, then exit\n"
		   "    show this help, the exit\n");
	printf("\n%s\n", report_bug_info);
}

static int
cmd_set_xid_status(int argc, char * argv[])
{
    int c;
	char        *commit_log_file = NULL;
    uint32      xid = 0;
    int         xid_status = 0;

	while ((c = getopt(argc-1, &argv[1], "hf:x:s:")) != -1)
	{
		switch (c)
		{
            case 'h':
                help_set_xid_status();
                return 0;
			case 'f':
                commit_log_file = optarg;
				break;
			case 'x':
				xid = atoi(optarg);
				break;
			case 's':
				xid_status = atoi(optarg);
				break;
			default:
				fprintf(stderr, 
                        "Try \"%s %s --help\" for more information.\n", 
                        progname, argv[1]);
				return 2;
				break;
		}
	}
    if (argc == 2)
	{
		fprintf(stderr, 
                "%s %s : not enough command-line arguments\n", 
                progname, argv[1]);
		return 2;
	}
    return set_xid_status(commit_log_file, xid, xid_status);
}

static void
help_get_xid_status(void)
{
	printf("Usage:\n");
	printf("  %s get_xid_status [OPTION]\n", progname);
	printf("\nOptions:\n");
	printf("  -f <commit_log_file>\n"
		   "    specify commit log file\n");
	printf("  -x <xid>\n"
		   "    Specifies xid that you want to get it's status\n");
	printf("  -h show this help, then exit\n"
		   "    show this help, the exit\n");
	printf("\n%s\n", report_bug_info);
}

static int
cmd_get_xid_status(int argc, char * argv[])
{
    int c;
	char        *commit_log_file = NULL;
    uint32      xid = 0;

	while ((c = getopt(argc-1, &argv[1], "hf:x:")) != -1)
	{
		switch (c)
		{
            case 'h':
                help_get_xid_status();
                return 0;
			case 'f':
                commit_log_file = optarg;
				break;
			case 'x':
				xid = atoi(optarg);
				break;
			default:
				fprintf(stderr, 
                        "Try \"%s %s --help\" for more information.\n", 
                        progname, argv[1]);
				return 2;
				break;
		}
	}
    if (argc == 2)
	{
		fprintf(stderr, 
                "%s %s : not enough command-line arguments\n", 
                progname, argv[1]);
		return 2;
	}
    return get_xid_status(commit_log_file, xid);
}

static void
usage(void)
{
	printf("Usage:\n");
	printf("  %s <cmd> [OPTION]\n", progname);
	printf("\ncmd:\n"
		   "  show_tuple: show tuples in a relation\n"
		   "  clean_tuple_hint: clean hint in tuple\n"
		   "  get_xid_status: get status of xid\n"
		   "  set_xid_status: set status of xid\n");
	printf("\nOptions:\n");
	printf("  -h: show this cmd  help, then exit\n");
	printf("\n%s\n", report_bug_info);
}

/*------------ MAIN ----------------------------------------*/
int
main(int argc, char **argv)
{
	progname = get_progname(argv[0]);

    if (argc == 1)
    {
        usage();
        exit(0);
    }
    else if (argc == 2)
	{
		if (strcmp(argv[1], "--help") == 0 
            || strcmp(argv[1], "-?") == 0
            || strcmp(argv[1], "-h") == 0)
		{
			usage();
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("pg_fix (PostgreSQL) " PG_VERSION);
			exit(0);
		}
	}

    if (!strcmp(argv[1], "show_tuple"))
    {
        return cmd_show_tuple(argc, argv);
    }
    else if (!strcmp(argv[1], "clean_tuple_hint"))
    {
        return cmd_clean_tuple_hint(argc, argv);
    }
    else if (!strcmp(argv[1], "get_xid_status"))
    {
        return cmd_get_xid_status(argc, argv);
    }
    else if (!strcmp(argv[1], "set_xid_status"))
    {
        return cmd_set_xid_status(argc, argv);
    }
    else
    {
		fprintf(stderr, "Unknown command %s\n", argv[1]);
    }
    return 0;
}
