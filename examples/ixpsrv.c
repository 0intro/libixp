/* Copyright ©2007 Ron Minnich <rminnich at gmail dot com>
/* Copyright ©2007-2010 Kris Maglione <maglione.k@gmail.com>
 * See LICENSE file for license details.
 */

/* This is a simple 9P file server which serves a normal filesystem
 * hierarchy. While some of the code is from wmii, the server is by
 * Ron.
 *
 * Note: I added an ifdef for Linux vs. BSD for the mount call, so
 * this compiles on BSD, but it won't actually run. It should,
 * ideally, have the option of not mounting the FS.
 *   --Kris
 */
#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <ixp_local.h>

/* Temporary */
#define fatal(...) ixp_eprint("ixpsrv: fatal: " __VA_ARGS__)
#define debug(...) if(debuglevel) fprintf(stderr, "ixpsrv: " __VA_ARGS__)

/* Datatypes: */
typedef struct FidAux FidAux;
struct FidAux {
	DIR *dir;
	int fd;
	char name[]; /* c99 */
};

/* Error messages */
static char
	Enoperm[] = "permission denied",
	Enofile[] = "file not found",
	Ebadvalue[] = "bad value";
/* Macros */
#define QID(t, i) ((int64_t)(t))

/* Global Vars */
static IxpServer server;
static char *user;
static int debuglevel = 0;

static char *argv0;

static void fs_open(Ixp9Req *r);
static void fs_walk(Ixp9Req *r);
static void fs_read(Ixp9Req *r);
static void fs_stat(Ixp9Req *r);
static void fs_write(Ixp9Req *r);
static void fs_clunk(Ixp9Req *r);
static void fs_flush(Ixp9Req *r);
static void fs_attach(Ixp9Req *r);
static void fs_create(Ixp9Req *r);
static void fs_remove(Ixp9Req *r);
static void fs_freefid(IxpFid *f);

Ixp9Srv p9srv = {
	.open=	fs_open,
	.walk=	fs_walk,
	.read=	fs_read,
	.stat=	fs_stat,
	.write=	fs_write,
	.clunk=	fs_clunk,
	.flush=	fs_flush,
	.attach=fs_attach,
	.create=fs_create,
	.remove=fs_remove,
	.freefid=fs_freefid
};

static void
usage() {
	fprintf(stderr,
		   "usage: %1$s [-a <address>] {create | read | ls [-ld] | remove | write} <file>\n"
		   "       %1$s [-a <address>] xwrite <file> <data>\n"
		   "       %1$s -v\n", argv0);
	exit(1);
}


/* Utility Functions */

static FidAux*
newfidaux(char *name) {
	FidAux *f;

	f = ixp_emallocz(sizeof(*f) + strlen(name) + 1);
	f->fd = -1;
	strcpy(f->name, name);
	return f;
}
/* is this a dir? */
/* -1 means it ain't anything .. */
static int
isdir(char *path) {
	struct stat buf;

	if (stat(path, &buf) < 0)
		return -1;

	return S_ISDIR(buf.st_mode);
}

static void
dostat(IxpStat *s, char *name, struct stat *buf) {

	s->type = 0;
	s->dev = 0;
	s->qid.type = buf->st_mode&S_IFMT;
	s->qid.path = buf->st_ino;
	s->qid.version = 0;
	s->mode = buf->st_mode & 0777;
	if (S_ISDIR(buf->st_mode)) {
		s->mode |= P9_DMDIR;
		s->qid.type |= QTDIR;
	}
	s->atime = buf->st_atime;
	s->mtime = buf->st_mtime;
	s->length = buf->st_size;
	s->name =name;
	s->uid = user;
	s->gid = user;
	s->muid = user;
}

/* the gnu/linux guys have made a real mess of errno ... don't ask --ron */
/* I agree. --Kris */
void
rerrno(Ixp9Req *r, char *m) {
/*
	char errbuf[128];
	ixp_respond(r, strerror_r(errno, errbuf, sizeof(errbuf)));
 */
	ixp_respond(r, m);
}

void
fs_attach(Ixp9Req *r) {

	debug("fs_attach(%p)\n", r);

	r->fid->qid.type = QTDIR;
	r->fid->qid.path = (uintptr_t)r->fid;
	r->fid->aux = newfidaux("/");
	r->ofcall.rattach.qid = r->fid->qid;
	ixp_respond(r, nil);
}

void
fs_walk(Ixp9Req *r) {
	struct stat buf;
	char *name;
	FidAux *f;
	int i;
	
	debug("fs_walk(%p)\n", r);

	f = r->fid->aux;
	name = malloc(PATH_MAX);
	strcpy(name, f->name);
	if (stat(name, &buf) < 0){
		ixp_respond(r, Enofile);
		return;
	}

	/* build full path. Stat full path. Done */
	for(i=0; i < r->ifcall.twalk.nwname; i++) {
		strcat(name, "/");
		strcat(name, r->ifcall.twalk.wname[i]);
		if (stat(name, &buf) < 0){
			ixp_respond(r, Enofile);
			free(name);
			return;
		}
		r->ofcall.rwalk.wqid[i].type = buf.st_mode&S_IFMT;
		r->ofcall.rwalk.wqid[i].path = buf.st_ino;
	}

	r->newfid->aux = newfidaux(name);
	r->ofcall.rwalk.nwqid = i;
	free(name);
	ixp_respond(r, nil);
}

void
fs_stat(Ixp9Req *r) {
	struct stat st;
	IxpStat s;
	IxpMsg m;
	char *name;
	char *buf;
	FidAux *f;
	int size;
	
	f = r->fid->aux;
	debug("fs_stat(%p)\n", r);
	debug("fs_stat %s\n", f->name);

	name = f->name;
	if (stat(name, &st) < 0){
		ixp_respond(r, Enofile);
		return;
	}

	dostat(&s, name, &st);
	r->fid->qid = s.qid;
	r->ofcall.rstat.nstat = size = ixp_sizeof_stat(&s);

	buf = ixp_emallocz(size);

	m = ixp_message(buf, size, MsgPack);
	ixp_pstat(&m, &s);

	r->ofcall.rstat.stat = m.data;
	ixp_respond(r, nil);
}

void
fs_read(Ixp9Req *r) {
	FidAux *f;
	char *buf;
	int n, offset;
	int size;

	debug("fs_read(%p)\n", r);

	f = r->fid->aux;

	if (f->dir) {
		IxpStat s;
		IxpMsg m;

		offset = 0;
		size = r->ifcall.tread.count;
		buf = ixp_emallocz(size);
		m = ixp_message(buf, size, MsgPack);

		/* note: we don't really handle lots of things well, so do one thing
		 * at a time 
		 */
		/*for(f=f->next; f; f=f->next) */{
			struct dirent *d;
			struct stat st;
			d = readdir(f->dir);
			if (d) {
				stat(d->d_name, &st);
				dostat(&s, d->d_name, &st);
				n = ixp_sizeof_stat(&s);
				ixp_pstat(&m, &s);
				offset += n;
			} else n = 0;
		}
		r->ofcall.rread.count = n;
		r->ofcall.rread.data = (char*)m.data;
		ixp_respond(r, nil);
		return;
	} else {
		r->ofcall.rread.data = ixp_emallocz(r->ifcall.tread.count);
		if (! r->ofcall.rread.data) {
			ixp_respond(r, nil);
			return;
		}
		r->ofcall.rread.count = pread(f->fd, r->ofcall.rread.data, r->ifcall.tread.count, r->ifcall.tread.offset);
		if (r->ofcall.rread.count < 0) 
			rerrno(r, Enoperm);
		else
			ixp_respond(r, nil);
		return;
	}

	/* 
	 * This is an assert because this should this should not be called if
	 * the file is not open for reading.
	 */
	assert(!"Read called on an unreadable file");
}

void
fs_write(Ixp9Req *r) {
	FidAux *f;

	debug("fs_write(%p)\n", r);

	if(r->ifcall.twrite.count == 0) {
		ixp_respond(r, nil);
		return;
	}
	f = r->fid->aux;
	/*
	 * This is an assert because this function should not be called if
	 * the file is not open for writing.
	 */
	assert(!"Write called on an unwritable file");
}

void
fs_open(Ixp9Req *r) {
	int dir;
	FidAux *f;

	debug("fs_open(%p)\n", r);

	f = r->fid->aux;
	dir = isdir(f->name);
	/* fucking stupid linux -- open dir is a DIR */

	if (dir) {
		f->dir = opendir(f->name);
		if (! f->dir){
			rerrno(r, Enoperm);
			return;
		}
	} else {
		f->fd = open(f->name, O_RDONLY);
		if (f->fd < 0){
			rerrno(r, Enoperm);
			return;
		}
	}
	ixp_respond(r, nil);
}


void
fs_create(Ixp9Req *r) {
	debug("fs_create(%p)\n", r);
	ixp_respond(r, Enoperm);
}

void
fs_remove(Ixp9Req *r) {
	debug("fs_remove(%p)\n", r);
	ixp_respond(r, Enoperm);

}

void
fs_clunk(Ixp9Req *r) {
	int dir;
	FidAux *f;

	debug("fs_clunk(%p)\n", f);

	f = r->fid->aux;
	dir = isdir(f->name);
	if (dir) {
		(void) closedir(f->dir);
		f->dir = NULL;
	} else {
		(void) close(f->fd);
		f->fd = -1;
	}

	ixp_respond(r, nil);
}

void
fs_flush(Ixp9Req *r) {
	debug("fs_flush(%p)\n", r);
	ixp_respond(r, nil);
}

void
fs_freefid(IxpFid *f) {
	debug("fs_freefid(%p)\n", f);
	free(f->aux);
}

// mount -t 9p 127.1 /tmp/cache -o port=20006,noextend
/* Yuck. */
#if defined(__linux__)
#  define MF(n) MS_##n
#  define mymount(src, dest, flags, opts) mount(src, dest, "9p", flags, opts)
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
#  define MF(n) MNT_##n
#  define mymount(src, dest, flags, opts) mount("9p", dest, flags, src)
#endif

static ulong mountflags =
	  MF(NOATIME)
	| MF(NODEV)
	/* | MF(NODIRATIME) */
	| MF(NOEXEC)
	| MF(NOSUID)
	| MF(RDONLY);

int
getaddr(char *mountaddr, char **ip, char **port) {
	char *cp;

	if (!mountaddr)
		mountaddr = getenv("XCPU_PARENT");
	if (!mountaddr)
		return -1;

	cp = mountaddr;
	if (strcmp(cp, "tcp!"))
		cp += 4;

	*ip = cp;

	cp = strstr(cp, "!");
	if (cp)
		*port = cp + 1;
	return strtoul(*port, 0, 0);
}

int
main(int argc, char *argv[]) {
	int fd;
	int ret;
	int domount, mountonly;
	char *mountaddr;
	char *address;
	char *msg;
	IxpConn *acceptor;

	domount = 0;
	mountonly = 0;
	address = getenv("IXP_ADDRESS");
	mountaddr = nil;

	ARGBEGIN{
	case 'v':
		printf("%s-" VERSION ", ©2007 Ron Minnich\n", argv0);
		exit(0);
	case 'a':
		address = EARGF(usage());
		break;
	case 'd':
		debuglevel++;
		break;
	case 'm':
		domount++;
		break;
	case 'M':
		mountonly++;
		break;
	default:
		usage();
	}ARGEND;

	if(!address)
		fatal("$IXP_ADDRESS not set\n");

	if(!(user = getenv("USER")))
		fatal("$USER not set\n");

	fd = ixp_announce(address);
	if(fd < 0)
		fatal("%s\n", errstr);

	/* set up a fake client so we can grap connects. */
	acceptor = ixp_listen(&server, fd, &p9srv, ixp_serve9conn, NULL);

	/* we might need to mount ourselves. The bit of complexity is the need to fork so 
	 * we can serve ourselves. We've done the listen so that's ok.
	 */
	if (domount){
		int f = fork();
		if (f < 0)
			errx(1, "fork!");
		if (!f){
			char *addr, *aport;
			int port;
			char options[128];

			port = getaddr(mountaddr, &addr, &aport);
			sprintf(options, "port=%d,noextend", port);
			if (mymount(addr, "/tmp/cache", mountflags, options) < 0)
				errx(1, "Mount failed");
		}
		
	}

	if (mountonly)
		exit(0);

	ixp_serverloop(&server);
	printf("msg %s\n", ixp_errbuf());
	return ret;
}

