/* Copyright ©2007-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "ixp_local.h"

#define nelem(ary) (sizeof(ary) / sizeof(*ary))

enum {
	RootFid = 1,
};

static int
min(int a, int b) {
	if(a < b)
		return a;
	return b;
}

static IxpCFid*
getfid(IxpClient *c) {
	IxpCFid *f;

	thread->lock(&c->lk);
	f = c->freefid;
	if(f != nil)
		c->freefid = f->next;
	else {
		f = emallocz(sizeof *f);
		f->client = c;
		f->fid = ++c->lastfid;
		thread->initmutex(&f->iolock);
	}
	f->next = nil;
	f->open = 0;
	thread->unlock(&c->lk);
	return f;
}

static void
putfid(IxpCFid *f) {
	IxpClient *c;

	c = f->client;
	thread->lock(&c->lk);
	if(f->fid == c->lastfid) {
		c->lastfid--;
		thread->mdestroy(&f->iolock);
		free(f);
	}else {
		f->next = c->freefid;
		c->freefid = f;
	}
	thread->unlock(&c->lk);
}

static int
dofcall(IxpClient *c, Fcall *fcall) {
	Fcall *ret;

	ret = muxrpc(c, fcall);
	if(ret == nil)
		return 0;
	if(ret->type == RError) {
		werrstr("%s", ret->ename);
		goto fail;
	}
	if(ret->type != (fcall->type^1)) {
		werrstr("received mismatched fcall");
		goto fail;
	}
	memcpy(fcall, ret, sizeof *fcall);
	free(ret);
	return 1;
fail:
	ixp_freefcall(fcall);
	free(ret);
	return 0;
}

/**
 * Function: ixp_unmount
 *
 * Unmounts the client P<c> and frees its data structures.
 */
void
ixp_unmount(IxpClient *c) {
	IxpCFid *f;

	shutdown(c->fd, SHUT_RDWR);
	close(c->fd);

	muxfree(c);

	while((f = c->freefid)) {
		c->freefid = f->next;
		thread->mdestroy(&f->iolock);
		free(f);
	}
	free(c->rmsg.data);
	free(c->wmsg.data);
	free(c);
}

static void
allocmsg(IxpClient *c, int n) {
	c->rmsg.size = n;
	c->wmsg.size = n;
	c->rmsg.data = erealloc(c->rmsg.data, n);
	c->wmsg.data = erealloc(c->wmsg.data, n);
}

/**
 * Function: ixp_mountfd
 * Function: ixp_mount
 * Function: ixp_nsmount
 *
 * Params:
 *	fd - A file descriptor which is already connected
 *	     to a 9P server.
 *	address - An address (in Plan 9 resource fomat) on
 *	          which to connect to a 9P server.
 *	name - The name of the socket in the process's canonical
 *	       namespace directory.
 *
 * Initiate a 9P connection with the server at P<address>,
 * connected to on P<fd>, or under the process's namespace
 * directory as P<name>.
 *
 * Returns:
 *	A pointer to a new 9P client.
 */

IxpClient*
ixp_mountfd(int fd) {
	IxpClient *c;
	Fcall fcall;

	c = emallocz(sizeof *c);
	c->fd = fd;

	muxinit(c);

	allocmsg(c, 256);
	c->lastfid = RootFid;
	/* Override tag matching on TVersion */
	c->mintag = IXP_NOTAG;
	c->maxtag = IXP_NOTAG+1;

	fcall.type = TVersion;
	fcall.msize = IXP_MAX_MSG;
	fcall.version = IXP_VERSION;

	if(dofcall(c, &fcall) == 0) {
		ixp_unmount(c);
		return nil;
	}

	if(strcmp(fcall.version, IXP_VERSION) || fcall.msize > IXP_MAX_MSG) {
		werrstr("bad 9P version response");
		ixp_unmount(c);
		return nil;
	}

	c->mintag = 0;
	c->maxtag = 255;
	c->msize = fcall.msize;

	allocmsg(c, fcall.msize);
	ixp_freefcall(&fcall);

	fcall.type = TAttach;
	fcall.fid = RootFid;
	fcall.afid = IXP_NOFID;
	fcall.uname = getenv("USER");
	fcall.aname = "";
	if(dofcall(c, &fcall) == 0) {
		ixp_unmount(c);
		return nil;
	}

	return c;
}

IxpClient*
ixp_mount(const char *address) {
	int fd;

	fd = ixp_dial(address);
	if(fd < 0)
		return nil;
	return ixp_mountfd(fd);
}

IxpClient*
ixp_nsmount(const char *name) {
	char *address;
	IxpClient *c;

	address = ixp_namespace();
	if(address)
		address = ixp_smprint("unix!%s/%s", address, name);
	if(address == nil)
		return nil;
	c = ixp_mount(address);
	free(address);
	return c;
}

static IxpCFid*
walk(IxpClient *c, const char *path) {
	IxpCFid *f;
	char *p;
	Fcall fcall;
	int n;

	p = estrdup(path);
	n = tokenize(fcall.wname, nelem(fcall.wname), p, '/');
	f = getfid(c);

	fcall.type = TWalk;
	fcall.fid = RootFid;
	fcall.nwname = n;
	fcall.newfid = f->fid;
	if(dofcall(c, &fcall) == 0)
		goto fail;
	if(fcall.nwqid < n)
		goto fail;

	f->qid = fcall.wqid[n-1];

	ixp_freefcall(&fcall);
	free(p);
	return f;
fail:
	putfid(f);
	return nil;
}

static IxpCFid*
walkdir(IxpClient *c, char *path, const char **rest) {
	char *p;

	p = path + strlen(path) - 1;
	assert(p >= path);
	while(*p == '/')
		*p-- = '\0';

	while((p > path) && (*p != '/'))
		p--;
	if(*p != '/') {
		werrstr("bad path");
		return nil;
	}

	*p++ = '\0';
	*rest = p;
	return walk(c, path);
}

static int
clunk(IxpCFid *f) {
	IxpClient *c;
	Fcall fcall;
	int ret;

	c = f->client;

	fcall.type = TClunk;
	fcall.fid = f->fid;
	ret = dofcall(c, &fcall);
	if(ret)
		putfid(f);
	ixp_freefcall(&fcall);
	return ret;
}

/**
 * Function: ixp_remove
 *
 * Params:
 *	path - The path of the file to remove.
 *
 * Removes a file or directory from the remote server.
 *
 * Returns:
 *	ixp_remove returns 0 on failure, 1 on success.
 */

int
ixp_remove(IxpClient *c, const char *path) {
	Fcall fcall;
	IxpCFid *f;
	int ret;

	if((f = walk(c, path)) == nil)
		return 0;

	fcall.type = TRemove;
	fcall.fid = f->fid;;
	ret = dofcall(c, &fcall);
	ixp_freefcall(&fcall);
	putfid(f);

	return ret;
}

static void
initfid(IxpCFid *f, Fcall *fcall) {
	f->open = 1;
	f->offset = 0;
	f->iounit = fcall->iounit;
	if(f->iounit == 0 || fcall->iounit > f->client->msize-24)
		f->iounit =  f->client->msize-24;
	f->qid = fcall->qid;
}

/**
 * Function: ixp_create
 * Function: ixp_open
 *
 * Params:
 *	path - The path of the file to open or create.
 *	perm - The permissions with which to create the new
 *	       file. These will be ANDed with those of the
 *	       parent directory by the server.
 *	mode - The file's open mode.
 *
 * ixp_open and ixp_create each open a file at P<path>.
 * P<mode> must include OREAD, OWRITE, or ORDWR, and may
 * include any of the modes specified in 9pmodes(3).
 * ixp_create, additionally, creates a file at P<path> if it
 * doesn't already exist.
 *
 * Returns:
 *	A pointer on which to operate on the newly
 * opened file.
 */

IxpCFid*
ixp_create(IxpClient *c, const char *path, uint perm, uchar mode) {
	Fcall fcall;
	IxpCFid *f;
	char *tpath;;

	tpath = estrdup(path);

	f = walkdir(c, tpath, &path);
	if(f == nil)
		goto done;

	fcall.type = TCreate;
	fcall.fid = f->fid;
	fcall.name = (char*)(uintptr_t)path;
	fcall.perm = perm;
	fcall.mode = mode;

	if(dofcall(c, &fcall) == 0) {
		clunk(f);
		f = nil;
		goto done;
	}

	initfid(f, &fcall);
	f->mode = mode;

	ixp_freefcall(&fcall);

done:
	free(tpath);
	return f;
}

IxpCFid*
ixp_open(IxpClient *c, const char *path, uchar mode) {
	Fcall fcall;
	IxpCFid *f;

	f = walk(c, path);
	if(f == nil)
		return nil;

	fcall.type = TOpen;
	fcall.fid = f->fid;
	fcall.mode = mode;

	if(dofcall(c, &fcall) == 0) {
		clunk(f);
		return nil;
	}

	initfid(f, &fcall);
	f->mode = mode;

	ixp_freefcall(&fcall);
	return f;
}

/**
 * Function: ixp_close
 *
 * Closes the file pointed to by P<f> and frees its
 * associated data structures;
 *
 * Returns:
 *	Returns 1 on success, and zero on failure.
 */

int
ixp_close(IxpCFid *f) {
	return clunk(f);
}

static Stat*
_stat(IxpClient *c, ulong fid) {
	IxpMsg msg;
	Fcall fcall;
	Stat *stat;

	fcall.type = TStat;
	fcall.fid = fid;
	if(dofcall(c, &fcall) == 0)
		return nil;

	msg = ixp_message((char*)fcall.stat, fcall.nstat, MsgUnpack);

	stat = emalloc(sizeof *stat);
	ixp_pstat(&msg, stat);
	ixp_freefcall(&fcall);
	if(msg.pos > msg.end) {
		free(stat);
		stat = nil;
	}
	return stat;
}

/**
 * Function: ixp_stat
 * Function: ixp_fstat
 *
 * Params:
 *	path - The path of the file to stat.
 *	f - A CFid of an open file to stat.
 *
 * Stats the file at P<path> or pointed to by P<f>.
 *
 * Returns:
 *	Returns a Stat structure, which must be freed by
 * the caller with free(3).
 *
 * S<Stat>
 */

Stat*
ixp_stat(IxpClient *c, const char *path) {
	Stat *stat;
	IxpCFid *f;

	f = walk(c, path);
	if(f == nil)
		return nil;

	stat = _stat(c, f->fid);
	clunk(f);
	return stat;
}

Stat*
ixp_fstat(IxpCFid *f) {
	return _stat(f->client, f->fid);
}

static long
_pread(IxpCFid *f, char *buf, long count, vlong offset) {
	Fcall fcall;
	int n, len;

	len = 0;
	while(len < count) {
		n = min(count-len, f->iounit);

		fcall.type = TRead;
		fcall.fid = f->fid;
		fcall.offset = offset;
		fcall.count = n;
		if(dofcall(f->client, &fcall) == 0)
			return -1;
		if(fcall.count > n)
			return -1;

		memcpy(buf+len, fcall.data, fcall.count);
		offset += fcall.count;
		len += fcall.count;

		ixp_freefcall(&fcall);
		if(fcall.count < n)
			break;
	}
	return len;
}

/**
 * Function: ixp_read
 * Function: ixp_pread
 *
 * Params:
 *	buf - A buffer in which to store the read data.
 *	count - The number of bytes to read.
 *	offset - The offset at which to begin reading.
 *
 * ixp_read and ixp_pread each read P<count> bytes of data
 * from the file pointed to by P<f>, into P<buf>. ixp_read
 * begins reading at its stored offset, and increments it by
 * the number of bytes read. ixp_pread reads beginning at
 * P<offset> and does not alter C<f>'s stored offset.
 *
 * Returns:
 *	These functions return the number of bytes read on
 * success and -1 on failure.
 */

long
ixp_read(IxpCFid *f, void *buf, long count) {
	int n;

	thread->lock(&f->iolock);
	n = _pread(f, buf, count, f->offset);
	if(n > 0)
		f->offset += n;
	thread->unlock(&f->iolock);
	return n;
}

long
ixp_pread(IxpCFid *f, void *buf, long count, vlong offset) {
	int n;

	thread->lock(&f->iolock);
	n = _pread(f, buf, count, offset);
	thread->unlock(&f->iolock);
	return n;
}

static long
_pwrite(IxpCFid *f, const void *buf, long count, vlong offset) {
	Fcall fcall;
	int n, len;

	len = 0;
	do {
		n = min(count-len, f->iounit);
		fcall.type = TWrite;
		fcall.fid = f->fid;
		fcall.offset = offset;
		fcall.data = (char*)buf + len;
		fcall.count = n;
		if(dofcall(f->client, &fcall) == 0)
			return -1;

		offset += fcall.count;
		len += fcall.count;

		ixp_freefcall(&fcall);
		if(fcall.count < n)
			break;
	} while(len < count);
	return len;
}

/**
 * Function: ixp_write
 * Function: ixp_pwrite
 *
 * Params:
 *	buf - A buffer holding the contents to store.
 *	count - The number of bytes to store.
 *	offset - The offset at which to write the data.
 *
 * ixp_write and ixp_pwrite each write P<count> bytes of
 * data stored in P<buf> to the file pointed to by C<f>.
 * ixp_write writes its data at its stored offset, and
 * increments it by P<count>. ixp_pwrite writes its data a
 * P<offset> and does not alter C<f>'s stored offset.
 *
 * Returns:
 *	These functions return the number of bytes actually
 * written. Any value less than P<count> must be considered
 * a failure.
 */

long
ixp_write(IxpCFid *f, const void *buf, long count) {
	int n;

	thread->lock(&f->iolock);
	n = _pwrite(f, buf, count, f->offset);
	if(n > 0)
		f->offset += n;
	thread->unlock(&f->iolock);
	return n;
}

long
ixp_pwrite(IxpCFid *f, const void *buf, long count, vlong offset) {
	int n;

	thread->lock(&f->iolock);
	n = _pwrite(f, buf, count, offset);
	thread->unlock(&f->iolock);
	return n;
}

/**
 * Function: ixp_vprint
 * Function: ixp_print
 * Variable: ixp_vsmprint
 *
 * Params:
 *	fmt - The string with which to format the data.
 *	ap - A va_list holding the arguments to the format
 *	     string.
 *	... - The arguments to the format string.
 *
 * These functions act like the standard formatted IO
 * functions. They write the result of the formatting to the
 * file pointed to by C<f>.
 *
 * V<ixp_vsmprint> may be set to a function which will
 * format its arguments and return a null terminated string
 * allocated with malloc(3).
 *
 * Returns:
 *	These functions return the number of bytes written.
 * There is currently no way to detect failure.
 */

int
ixp_vprint(IxpCFid *f, const char *fmt, va_list ap) {
	char *buf;
	int n;

	buf = ixp_vsmprint(fmt, ap);
	if(buf == nil)
		return -1;

	n = ixp_write(f, buf, strlen(buf));
	free(buf);
	return n;
}

int
ixp_print(IxpCFid *f, const char *fmt, ...) {
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = ixp_vprint(f, fmt, ap);
	va_end(ap);

	return n;
}

