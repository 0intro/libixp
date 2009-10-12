/* Copyright ©2006-2008 Kris Maglione <fbsdaemon at gmail dot com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "ixp_local.h"

typedef void*	IxpFileIdU;

static char
	Enofile[] = "file not found";

#include "ixp_srvutil.h"

struct IxpQueue {
	IxpQueue*	link;
	char*		dat;
	long		len;
};

/* Macros */
#define QID(t, i) (((vlong)((t)&0xFF)<<32)|((i)&0xFFFFFFFF))

/* Global Vars */
/***************/
static IxpFileId*	free_fileid;

/* Utility Functions */
/**
 * Obtain an empty, reference counted IxpFileId struct.
 */
IxpFileId*
ixp_srv_getfile(void) {
	IxpFileId *file;
	int i;

	if(!free_fileid) {
		i = 15;
		file = emallocz(i * sizeof *file);
		for(; i; i--) {
			file->next = free_fileid;
			free_fileid = file++;
		}
	}
	file = free_fileid;
	free_fileid = file->next;
	file->p = nil;
	file->volatil = 0;
	file->nref = 1;
	file->next = nil;
	file->pending = false;
	return file;
}

/**
 * Decrease the reference count of the given IxpFileId,
 * and push it onto the free list when it reaches 0;
 */
void
ixp_srv_freefile(IxpFileId *f) {
	if(--f->nref)
		return;
	free(f->tab.name);
	f->next = free_fileid;
	free_fileid = f;
}

/**
 * Increase the reference count of every IxpFileId linked
 * to 'f'.
 */
void
ixp_srv_clonefiles(IxpFileId *f) {
	for(; f; f=f->next)
		assert(f->nref++);
}

void
ixp_srv_readbuf(Ixp9Req *req, char *buf, uint len) {

	if(req->ifcall.io.offset >= len)
		return;

	len -= req->ifcall.io.offset;
	if(len > req->ifcall.io.count)
		len = req->ifcall.io.count;
	req->ofcall.io.data = emalloc(len);
	memcpy(req->ofcall.io.data, buf + req->ifcall.io.offset, len);
	req->ofcall.io.count = len;
}

void
ixp_srv_writebuf(Ixp9Req *req, char **buf, uint *len, uint max) {
	IxpFileId *file;
	char *p;
	uint offset, count;

	file = req->fid->aux;

	offset = req->ifcall.io.offset;
	if(file->tab.perm & DMAPPEND)
		offset = *len;

	if(offset > *len || req->ifcall.io.count == 0) {
		req->ofcall.io.count = 0;
		return;
	}

	count = req->ifcall.io.count;
	if(max && (offset + count > max))
		count = max - offset;

	*len = offset + count;
	if(max == 0)
		*buf = erealloc(*buf, *len + 1);
	p = *buf;

	memcpy(p+offset, req->ifcall.io.data, count);
	req->ofcall.io.count = count;
	p[offset+count] = '\0';
}

/**
 * Ensure that the data member of 'r' is null terminated,
 * removing any new line from its end.
 */
void
ixp_srv_data2cstring(Ixp9Req *req) {
	char *p, *q;
	uint i;

	i = req->ifcall.io.count;
	p = req->ifcall.io.data;
	if(i && p[i - 1] == '\n')
		i--;
	q = memchr(p, '\0', i);
	if(q)
		i = q - p;

	p = erealloc(req->ifcall.io.data, i+1);
	p[i] = '\0';
	req->ifcall.io.data = p;
}

char*
ixp_srv_writectl(Ixp9Req *req, char* (*fn)(void*, IxpMsg*)) {
	char *err, *s, *p, c;
	IxpFileId *file;
	IxpMsg msg;

	file = req->fid->aux;

	ixp_srv_data2cstring(req);
	s = req->ifcall.io.data;

	err = nil;
	c = *s;
	while(c != '\0') {
		while(*s == '\n')
			s++;
		p = s;
		while(*p != '\0' && *p != '\n')
			p++;
		c = *p;
		*p = '\0';

		msg = ixp_message(s, p-s, 0);
		s = fn(file->p, &msg);
		if(s)
			err = s;
		s = p + 1;
	}
	return err;
}

void
ixp_pending_respond(Ixp9Req *req) {
	IxpFileId *file;
	IxpPendingLink *p;
	IxpRequestLink *req_link;
	IxpQueue *queue;

	file = req->fid->aux;
	assert(file->pending);
	p = file->p;
	if(p->queue) {
		queue = p->queue;
		p->queue = queue->link;
		req->ofcall.io.data = queue->dat;
		req->ofcall.io.count = queue->len;
		if(req->aux) {
			req_link = req->aux;
			req_link->next->prev = req_link->prev;
			req_link->prev->next = req_link->next;
			free(req_link);
		}
		respond(req, nil);
		free(queue);
	}else {
		req_link = emallocz(sizeof *req_link);
		req_link->req = req;
		req_link->next = &p->pending->req;
		req_link->prev = req_link->next->prev;
		req_link->next->prev = req_link;
		req_link->prev->next = req_link;
		req->aux = req_link;
	}
}

void
ixp_pending_write(IxpPending *pending, char *dat, long n) {
	IxpRequestLink req_link;
	IxpQueue **qp, *queue;
	IxpPendingLink *pp;
	IxpRequestLink *rp;

	if(n == 0)
		return;

	if(pending->req.next == nil) {
		pending->req.next = &pending->req;
		pending->req.prev = &pending->req;
		pending->fids.prev = &pending->fids;
		pending->fids.next = &pending->fids;
	}

	for(pp=pending->fids.next; pp != &pending->fids; pp=pp->next) {
		for(qp=&pp->queue; *qp; qp=&qp[0]->link)
			;
		queue = emallocz(sizeof *queue);
		queue->dat = emalloc(n);
		memcpy(queue->dat, dat, n);
		queue->len = n;
		*qp = queue;
	}

	req_link.next = &req_link;
	req_link.prev = &req_link;
	if(pending->req.next != &pending->req) {
		req_link.next = pending->req.next;
		req_link.prev = pending->req.prev;
		pending->req.prev = &pending->req;
		pending->req.next = &pending->req;
	}
	req_link.prev->next = &req_link;
	req_link.next->prev = &req_link;

	while((rp = req_link.next) != &req_link)
		ixp_pending_respond(rp->req);
}

void
ixp_pending_pushfid(IxpPending *pending, IxpFid *fid) {
	IxpPendingLink *pend_link;
	IxpFileId *file;

	if(pending->req.next == nil) {
		pending->req.next = &pending->req;
		pending->req.prev = &pending->req;
		pending->fids.prev = &pending->fids;
		pending->fids.next = &pending->fids;
	}

	file = fid->aux;
	pend_link = emallocz(sizeof *pend_link);
	pend_link->fid = fid;
	pend_link->pending = pending;
	pend_link->next = &pending->fids;
	pend_link->prev = pend_link->next->prev;
	pend_link->next->prev = pend_link;
	pend_link->prev->next = pend_link;
	file->pending = true;
	file->p = pend_link;
}

static void
pending_flush(Ixp9Req *req) {
	IxpFileId *file;
	IxpRequestLink *req_link;

	file = req->fid->aux;
	if(file->pending) {
		req_link = req->aux;
		if(req_link) {
			req_link->prev->next = req_link->next;
			req_link->next->prev = req_link->prev;
			free(req_link);
		}
	}
}

void
ixp_pending_flush(Ixp9Req *req) {

	pending_flush(req->oldreq);
}

bool
ixp_pending_clunk(Ixp9Req *req) {
	IxpPending *pending;
	IxpPendingLink *pend_link;
	IxpRequestLink *req_link;
	Ixp9Req *r;
	IxpFileId *file;
	IxpQueue *queue;
	bool more;

	file = req->fid->aux;
	pend_link = file->p;

	pending = pend_link->pending;
	for(req_link=pending->req.next; req_link != &pending->req;) {
		r = req_link->req;
		req_link = req_link->next;
		if(r->fid == pend_link->fid) {
			pending_flush(r);
			respond(r, "interrupted");
		}
	}

	pend_link->prev->next = pend_link->next;
	pend_link->next->prev = pend_link->prev;

	while((queue = pend_link->queue)) {
		pend_link->queue = queue->link;
		free(queue->dat);
		free(queue);
	}
	more = (pend_link->pending->fids.next == &pend_link->pending->fids);
	free(pend_link);
	respond(req, nil);
	return more;
}

bool
ixp_srv_verifyfile(IxpFileId *file, IxpLookupFn lookup) {
	IxpFileId *tfile;
	int ret;

	if(!file->next)
		return true;

	ret = false;
	if(ixp_srv_verifyfile(file->next, lookup)) {
		tfile = lookup(file->next, file->tab.name);
		if(tfile) {
			if(!tfile->volatil || tfile->p == file->p)
				ret = true;
			ixp_srv_freefile(tfile);
		}
	}
	return ret;
}

void
ixp_srv_readdir(Ixp9Req *req, IxpLookupFn lookup, void (*dostat)(IxpStat*, IxpFileId*)) {
	IxpMsg msg;
	IxpFileId *file, *tfile;
	IxpStat stat;
	char *buf;
	ulong size, n;
	uvlong offset;

	file = req->fid->aux;

	size = req->ifcall.io.count;
	if(size > req->fid->iounit)
		size = req->fid->iounit;
	buf = emallocz(size);
	msg = ixp_message(buf, size, MsgPack);

	file = lookup(file, nil);
	tfile = file;
	/* Note: The first file is ".", so we skip it. */
	offset = 0;
	for(file=file->next; file; file=file->next) {
		dostat(&stat, file);
		n = ixp_sizeof_stat(&stat);
		if(offset >= req->ifcall.io.offset) {
			if(size < n)
				break;
			ixp_pstat(&msg, &stat);
			size -= n;
		}
		offset += n;
	}
	while((file = tfile)) {
		tfile=tfile->next;
		ixp_srv_freefile(file);
	}
	req->ofcall.io.count = msg.pos - msg.data;
	req->ofcall.io.data = msg.data;
	respond(req, nil);
}

void
ixp_srv_walkandclone(Ixp9Req *req, IxpLookupFn lookup) {
	IxpFileId *file, *tfile;
	int i;

	file = req->fid->aux;
	ixp_srv_clonefiles(file);
	for(i=0; i < req->ifcall.twalk.nwname; i++) {
		if(!strcmp(req->ifcall.twalk.wname[i], "..")) {
			if(file->next) {
				tfile=file;
				file=file->next;
				ixp_srv_freefile(tfile);
			}
		}else{
			tfile = lookup(file, req->ifcall.twalk.wname[i]);
			if(!tfile)
				break;
			assert(!tfile->next);
			if(strcmp(req->ifcall.twalk.wname[i], ".")) {
				tfile->next = file;
				file = tfile;
			}
		}
		req->ofcall.rwalk.wqid[i].type = file->tab.qtype;
		req->ofcall.rwalk.wqid[i].path = QID(file->tab.type, file->id);
	}
	/* There should be a way to do this on freefid() */
	if(i < req->ifcall.twalk.nwname) {
		while((tfile = file)) {
			file=file->next;
			ixp_srv_freefile(tfile);
		}
		respond(req, Enofile);
		return;
	}
	/* Remove refs for req->fid if no new fid */
	if(req->ifcall.hdr.fid == req->ifcall.twalk.newfid) {
		tfile = req->fid->aux;
		req->fid->aux = file;
		while((file = tfile)) {
			tfile = tfile->next;
			ixp_srv_freefile(file);
		}
	}else
		req->newfid->aux = file;
	req->ofcall.rwalk.nwqid = i;
	respond(req, nil);
}

