/* Copyright ©2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <stdarg.h>
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

static IxpCFid *
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
	memcpy(fcall, ret, sizeof(*fcall));
	free(ret);
	return 1;
fail:
	ixp_freefcall(fcall);
	free(ret);
	return 0;
}

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

IxpClient *
ixp_mountfd(int fd) {
	IxpClient *c;
	Fcall fcall;

	c = emallocz(sizeof(*c));
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

IxpClient *
ixp_mount(char *address) {
	int fd;

	fd = ixp_dial(address);
	if(fd < 0)
		return nil;
	return ixp_mountfd(fd);
}

static IxpCFid *
walk(IxpClient *c, char *path) {
	IxpCFid *f;
	Fcall fcall;
	int n;

	path = estrdup(path);
	n = tokenize(fcall.wname, nelem(fcall.wname), path, '/');
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
	free(path);
	return f;
fail:
	putfid(f);
	return nil;
}

static IxpCFid *
walkdir(IxpClient *c, char *path, char **rest) {
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

int
ixp_remove(IxpClient *c, char *path) {
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

IxpCFid*
ixp_create(IxpClient *c, char *name, uint perm, uchar mode) {
	Fcall fcall;
	IxpCFid *f;
	char *path;;

	path = estrdup(name);

	f = walkdir(c, path, &name);
	if(f == nil)
		goto done;

	fcall.type = TCreate;
	fcall.fid = f->fid;
	fcall.name = name;
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
	free(path);
	return f;
}

IxpCFid*
ixp_open(IxpClient *c, char *name, uchar mode) {
	Fcall fcall;
	IxpCFid *f;

	f = walk(c, name);
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

int
ixp_close(IxpCFid *f) {
	return clunk(f);
}

Stat *
ixp_stat(IxpClient *c, char *path) {
	IxpMsg msg;
	Fcall fcall;
	Stat *stat;
	IxpCFid *f;

	stat = nil;
	f = walk(c, path);
	if(f == nil)
		return nil;

	fcall.type = TStat;
	fcall.fid = f->fid;
	if(dofcall(c, &fcall) == 0)
		goto done;

	msg = ixp_message(fcall.stat, fcall.nstat, MsgUnpack);

	stat = emalloc(sizeof(*stat));
	ixp_pstat(&msg, stat);
	ixp_freefcall(&fcall);
	if(msg.pos > msg.end) {
		free(stat);
		stat = 0;
	}

done:
	clunk(f);
	return stat;
}

static long
_pread(IxpCFid *f, void *buf, long count, vlong offset) {
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
_pwrite(IxpCFid *f, void *buf, long count, vlong offset) {
	Fcall fcall;
	int n, len;

	len = 0;
	do {
		n = min(count-len, f->iounit);
		fcall.type = TWrite;
		fcall.fid = f->fid;
		fcall.offset = offset;
		fcall.data = buf + len;
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

long
ixp_write(IxpCFid *f, void *buf, long count) {
	int n;

	thread->lock(&f->iolock);
	n = _pwrite(f, buf, count, f->offset);
	if(n > 0)
		f->offset += n;
	thread->unlock(&f->iolock);
	return n;
}

long
ixp_pwrite(IxpCFid *f, void *buf, long count, vlong offset) {
	int n;

	thread->lock(&f->iolock);
	n = _pwrite(f, buf, count, offset);
	thread->unlock(&f->iolock);
	return n;
}

int
ixp_vprint(IxpCFid *f, char *fmt, va_list ap) {
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
ixp_print(IxpCFid *f, char *fmt, ...) {
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = ixp_vprint(f, fmt, ap);
	va_end(ap);

	return n;
}

