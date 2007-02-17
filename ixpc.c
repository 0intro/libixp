/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include <ixp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

static IXPClient c = { 0 };
static char buffer[1024] = { 0 };

static void
write_data(unsigned int fid) {
	void *data = ixp_emallocz(c.ofcall.iounit);
	unsigned long long offset = 0;
	unsigned int len = 0;

	while((len = read(0, data, c.ofcall.iounit)) > 0) {
		if(ixp_client_write(&c, fid, offset, len, data) != len) {
			fprintf(stderr, "ixpc: cannot write file: %s\n", c.errstr);
			break;
		}
		offset += len;
	}
	if(offset == 0) /* do an explicit empty write when no writing has been done yet */
		if(ixp_client_write(&c, fid, offset, 0, 0) != 0)
			fprintf(stderr, "ixpc: cannot write file: %s\n", c.errstr);
	free(data);
}

static int
xcreate(char *file) {
	unsigned int fid;
	char *p = strrchr(file, '/');

	if(!p)
		p = file;
	fid = c.root_fid << 2;
	/* walk to bottom-most directory */
	*p = 0;
	if(ixp_client_walk(&c, fid, file) == -1) {
		fprintf(stderr, "ixpc: cannot walk to '%s': %s\n", file, c.errstr);
		return -1;
	}
	p++;
	if(ixp_client_create(&c, fid, p, IXP_DMWRITE, IXP_OWRITE) == -1) {
		fprintf(stderr, "ixpc: cannot create file '%s': %s\n", p, c.errstr);
		return -1;
	}
	if(!(c.ofcall.qid.type&P9DMDIR))
		write_data(fid);
	return ixp_client_close(&c, fid);
}

static int
xwrite(char *file, unsigned char mode) {
	/* open */
	unsigned int fid = c.root_fid << 2;
	if(ixp_client_walkopen(&c, fid, file, mode) == -1) {
		fprintf(stderr, "ixpc: cannot open file '%s': %s\n", file, c.errstr);
		return -1;
	}
	write_data(fid);
	return ixp_client_close(&c, fid);
}

static int
xawrite(char *file, unsigned char mode) {
	/* open */
	unsigned int fid = c.root_fid << 2;
	if(ixp_client_walkopen(&c, fid, file, mode) == -1) {
		fprintf(stderr, "ixpc: cannot open file '%s': %s\n", file, c.errstr);
		return -1;
	}
	if(ixp_client_write(&c, fid, 0, strlen(buffer), buffer) != strlen(buffer))
		fprintf(stderr, "ixpc: cannot write file: %s\n", c.errstr);
	return ixp_client_close(&c, fid);
}

static int
comp_stat(const void *s1, const void *s2) {
	Stat *st1 = (Stat *)s1;
	Stat *st2 = (Stat *)s2;
	return strcmp(st1->name, st2->name);
}

static void
setrwx(long m, char *s) {
	static char *modes[] = {
		"---",
		"--x",
		"-w-",
		"-wx",
		"r--",
		"r-x",
		"rw-",
		"rwx",
	};
	strncpy(s, modes[m], 3);
}

static char *
str_of_mode(unsigned int mode) {
	static char buf[16];

	if(mode & IXP_DMDIR)
		buf[0]='d';
	else
		buf[0]='-';
	buf[1]='-';
	setrwx((mode >> 6) & 7, &buf[2]);
	setrwx((mode >> 3) & 7, &buf[5]);
	setrwx((mode >> 0) & 7, &buf[8]);
	buf[11] = 0;
	return buf;
}

static char *
str_of_time(unsigned int val) {
	static char buf[32];
	time_t t = (time_t)(int)val;
	char *tstr = ctime(&t);

	strncpy(buf, tstr ? tstr : "in v a l id ", sizeof(buf));
	buf[strlen(buf) - 1] = 0;
	return buf;
}

static void
print_stat(Stat *s, int details) {
	if(details)
		fprintf(stdout, "%s %s %s %5llu %s %s\n", str_of_mode(s->mode),
				s->uid, s->gid, s->length, str_of_time(s->mtime), s->name);
	else {
		if(s->mode & IXP_DMDIR)
			fprintf(stdout, "%s/\n", s->name);
		else
			fprintf(stdout, "%s\n", s->name);
	}
}

static void
xls(void *result, unsigned int msize, int details) {
	unsigned int n = 0, i = 0;
	unsigned char *p = result;
	Stat *dir;
	static Stat stat;

	do {
		ixp_unpack_stat(&p, NULL, &stat);
		n++;
	}
	while(p - (unsigned char*)result < msize);
	dir = (Stat *)ixp_emallocz(sizeof(Stat) * n);
	p = result;
	do {
		ixp_unpack_stat(&p, NULL, &dir[i++]);
	}
	while(p - (unsigned char*)result < msize);
	qsort(dir, n, sizeof(Stat), comp_stat);
	for(i = 0; i < n; i++)
		print_stat(&dir[i], details);
	free(dir);
	fflush(stdout);
}

static int
xdir(char *file, int details) {
	unsigned int fid = c.root_fid << 2;
	/* XXX: buffer overflow */
	Stat *s = ixp_emallocz(sizeof(Stat));
	unsigned char *buf;
	int count;
	static unsigned char result[IXP_MAX_MSG];
	void *data = NULL;
	unsigned long long offset = 0;

	if(ixp_client_stat(&c, fid, file) == -1) {
		fprintf(stderr, "ixpc: cannot stat file '%s': %s\n", file, c.errstr);
		return -1;
	}
	buf = c.ofcall.stat;
	ixp_unpack_stat(&buf, NULL, s);
	if(!(s->mode & IXP_DMDIR)) {
		print_stat(s, details);
		fflush(stdout);
		return 0;
	}
	/* directory */
	if(ixp_client_open(&c, fid, IXP_OREAD) == -1) {
		fprintf(stderr, "ixpc: cannot open directory '%s': %s\n", file, c.errstr);
		return -1;
	}
	while((count = ixp_client_read(&c, fid, offset, result, IXP_MAX_MSG)) > 0) {
		data = ixp_erealloc(data, offset + count);
		memcpy(data + offset, result, count);
		offset += count;
	}
	if(count == -1) {
		fprintf(stderr, "ixpc: cannot read directory '%s': %s\n", file, c.errstr);
		return -1;
	}
	if(data)
		xls(data, offset + count, details);
	return ixp_client_close(&c, fid);
}

static int
xread(char *file) {
	unsigned int fid = c.root_fid << 2;
	int count;
	static unsigned char result[IXP_MAX_MSG];
	unsigned long long offset = 0;

	if(ixp_client_walkopen(&c, fid, file, IXP_OREAD) == -1) {
		fprintf(stderr, "ixpc: cannot open file '%s': %s\n", file, c.errstr);
		return -1;
	}
	while((count = ixp_client_read(&c, fid, offset, result, IXP_MAX_MSG)) > 0) {
		write(1, result, count);
		offset += count;
	}
	if(count == -1) {
		fprintf(stderr, "ixpc: cannot read file/directory '%s': %s\n", file, c.errstr);
		return -1;
	}
	return ixp_client_close(&c, fid);
}

static int
xremove(char *file) {
	unsigned int fid;

	fid = c.root_fid << 2;
	if(ixp_client_remove(&c, fid, file) == -1) {
		fprintf(stderr, "ixpc: cannot remove file '%s': %s\n", file, c.errstr);
		return -1;
	}
	return 0;
}

int
main(int argc, char *argv[]) {
	int ret = 0, i = 0, details = 0;
	char *cmd, *file, *address = getenv("IXP_ADDRESS");

	/* command line args */
	if(argc < 2)
		goto Usage;
	for(i = 1; i < argc && argv[i][0] == '-'; i++)
		if(!strncmp(argv[i], "-v", 3)) {
			fputs("ixpc-"VERSION", (C)opyright MMIV-MMVI Anselm R. Garbe\n", stdout);
			exit(EXIT_SUCCESS);
		}
		else if(!strncmp(argv[i], "-a", 3))
			address = argv[++i];
	if(i + 2 > argc)
		goto Usage;
	cmd = argv[i++];
	if(!strcmp(argv[i], "-l")) {
		details = 1;
		if(++i >= argc || strcmp(cmd, "ls"))
			goto Usage;
	}
	file = argv[i++];
	if(!address)
		ixp_eprint("ixpc: error: $IXP_ADDRESS not set\n");
	if(ixp_client_dial(&c, address, getpid()) == -1)
		ixp_eprint("ixpc: %s\n", c.errstr);
	if(!strncmp(cmd, "create", 7))
		ret = xcreate(file);
	else if(!strncmp(cmd, "ls", 3))
		ret = xdir(file, details);
	else if(!strncmp(cmd, "read", 5))
		ret = xread(file);
	else if(!strncmp(cmd, "remove", 7))
		ret = xremove(file);
	else if(!strncmp(cmd, "write", 6))
		ret = xwrite(file, IXP_OWRITE);
	else if(!strncmp(cmd, "xwrite", 7)) {
		if(i < argc)
			ixp_strlcat(buffer, argv[i++], 1023);
		while(i < argc) {
			ixp_strlcat(buffer, " ", 1024);
			if(ixp_strlcat(buffer, argv[i++], 1024) > 1023)
				break;
		}
		ret = xawrite(file, IXP_OWRITE);
	}else {
Usage:
		ixp_eprint("usage: ixpc [-a <address>] {create | read | ls [-l] | remove | write} <file>\n"
		           "       ixpc [-a <address>] xwrite <file> <data>\n"
		           "       ixpc -v\n");
	}
	/* close socket */
	ixp_client_hangup(&c);
	return ret;
}
