/* Copyright ©2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "ixp.h"

#define nelem(ary) (sizeof(ary) / sizeof(*ary))
static char errbuf[1024];

enum {
	RootFid = 1,
	Tag = 1,
};

static int
min(int a, int b) {
	if(a < b)
		return a;
	return b;
}

static IxpCFid *
getfid(IxpClient *c) {
	IxpCFid *temp;
	uint i;

	temp = c->freefid;
	if(temp != nil)
		c->freefid = temp->next;
	else {
		temp = ixp_emallocz(sizeof(IxpCFid) * i);
		temp->client = c;
		temp->fid = ++c->lastfid;
	}
	temp->next = nil;
	temp->open = 0;
	return temp;
}

static void
putfid(IxpCFid *f) {
	IxpClient *c;

	c = f->client;
	f->next = c->freefid;
	c->freefid = f;
}

static int
dofcall(IxpClient *c, Fcall *fcall) {
	int type;

	type = fcall->type;
	if(ixp_fcall2msg(&c->msg, fcall) == 0) {
		errstr = "failed to pack message";
		return 0;
	}
	if(ixp_sendmsg(c->fd, &c->msg) == 0)
		return 0;
	if(ixp_recvmsg(c->fd, &c->msg) == 0)
		return 0;
	if(ixp_msg2fcall(&c->msg, fcall) == 0) {
		errstr = "received bad message";
		return 0;
	}
	if(fcall->type == RError) {
		strncpy(errbuf, fcall->ename, sizeof errbuf);
		ixp_freefcall(fcall);
		errstr = errbuf;
		return 0;
	}
	if(fcall->type != (type^1)) {
		ixp_freefcall(fcall);
		errstr = "received mismatched fcall";
		return 0;
	}
	return 1;
}

void
ixp_unmount(IxpClient *c) {
	IxpCFid *f;

	shutdown(c->fd, SHUT_RDWR);
	close(c->fd);

	while((f = c->freefid)) {
		c->freefid = f->next;
		free(f);
	}
	free(c->msg.data);
	free(c);
}

IxpClient *
ixp_mountfd(int fd) {
	IxpClient *c;
	Fcall fcall;

	c = ixp_emallocz(sizeof(*c));
	c->fd = fd;

	c->msg.size = 64;
	c->msg.data = ixp_emalloc(c->msg.size);

	fcall.type = TVersion;
	fcall.tag = IXP_NOTAG;
	fcall.msize = IXP_MAX_MSG;
	fcall.version = IXP_VERSION;

	if(dofcall(c, &fcall) == 0) {
		ixp_unmount(c);
		return nil;
	}

	if(strcmp(fcall.version, IXP_VERSION) != 0) {
		errstr = "bad 9P version response";
		ixp_unmount(c);
		return nil;
	}

	c->msg.size = fcall.msize;
	c->msg.data = ixp_erealloc(c->msg.data, c->msg.size);
	ixp_freefcall(&fcall);

	fcall.type = TAttach;
	fcall.tag = Tag;
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

	path = ixp_estrdup(path);
	n = ixp_tokenize(fcall.wname, nelem(fcall.wname), path, '/');
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
		errstr = "bad path";
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
	fcall.tag = Tag;
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
	fcall.tag = Tag;
	fcall.fid = f->fid;;
	ret = dofcall(c, &fcall);
	ixp_freefcall(&fcall);
	putfid(f);

	return ret;
}

void
initfid(IxpCFid *f, Fcall *fcall) {
	f->open = 1;
	f->offset = 0;
	f->iounit = min(fcall->iounit, IXP_MAX_MSG-32);
	f->qid = fcall->qid;
}

IxpCFid *
ixp_create(IxpClient *c, char *name, uint perm, uchar mode) {
	Fcall fcall;
	IxpCFid *f;
	char *path;;

	path = ixp_estrdup(name);

	f = walkdir(c, path, &name);
	if(f == nil)
		goto done;

	fcall.type = TCreate;
	fcall.tag = Tag;
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

IxpCFid *
ixp_open(IxpClient *c, char *name, uchar mode) {
	Fcall fcall;
	IxpCFid *f;

	f = walk(c, name);
	if(f == nil)
		return nil;

	fcall.type = TOpen;
	fcall.tag = Tag;
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
	Message msg;
	Fcall fcall;
	Stat *stat;
	IxpCFid *f;

	stat = nil;
	f = walk(c, path);
	if(f == nil)
		return nil;

	fcall.type = TStat;
	fcall.tag = Tag;
	fcall.fid = f->fid;
	if(dofcall(c, &fcall) == 0)
		goto done;

	msg = ixp_message(fcall.stat, fcall.nstat, MsgUnpack);

	stat = ixp_emalloc(sizeof(*stat));
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

int
ixp_read(IxpCFid *f, void *buf, uint count) {
	Fcall fcall;
	int n, len;

	len = 0;
	while(len < count) {
		n = min(count-len, f->iounit);

		fcall.type = TRead;
		fcall.tag = IXP_NOTAG;
		fcall.fid = f->fid;
		fcall.offset = f->offset;
		fcall.count = n;
		if(dofcall(f->client, &fcall) == 0)
			return -1;
		if(fcall.count > n)
			return -1;

		memcpy(buf+len, fcall.data, fcall.count);
		f->offset += fcall.count;
		len += fcall.count;

		ixp_freefcall(&fcall);
		if(fcall.count < n)
			break;
	}
	return len;
}

int
ixp_write(IxpCFid *f, void *buf, uint count) {
	Fcall fcall;
	int n, len;

	len = 0;
	do {
		n = min(count-len, f->iounit);
		fcall.type = TWrite;
		fcall.tag = IXP_NOTAG;
		fcall.fid = f->fid;
		fcall.offset = f->offset;
		fcall.data = (uchar*)buf + len;
		fcall.count = n;
		if(dofcall(f->client, &fcall) == 0)
			return -1;

		f->offset += fcall.count;
		len += fcall.count;

		ixp_freefcall(&fcall);
		if(fcall.count < n)
			break;
	} while(len < count);
	return len;
}
