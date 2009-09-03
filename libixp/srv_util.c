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
	IxpFileId *f;
	int i;

	if(!free_fileid) {
		i = 15;
		f = emallocz(i * sizeof *f);
		for(; i; i--) {
			f->next = free_fileid;
			free_fileid = f++;
		}
	}
	f = free_fileid;
	free_fileid = f->next;
	f->p = nil;
	f->volatil = 0;
	f->nref = 1;
	f->next = nil;
	f->pending = false;
	return f;
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
ixp_srv_readbuf(Ixp9Req *r, char *buf, uint len) {

	if(r->ifcall.io.offset >= len)
		return;

	len -= r->ifcall.io.offset;
	if(len > r->ifcall.io.count)
		len = r->ifcall.io.count;
	r->ofcall.io.data = emalloc(len);
	memcpy(r->ofcall.io.data, buf + r->ifcall.io.offset, len);
	r->ofcall.io.count = len;
}

void
ixp_srv_writebuf(Ixp9Req *r, char **buf, uint *len, uint max) {
	IxpFileId *f;
	char *p;
	uint offset, count;

	f = r->fid->aux;

	offset = r->ifcall.io.offset;
	if(f->tab.perm & DMAPPEND)
		offset = *len;

	if(offset > *len || r->ifcall.io.count == 0) {
		r->ofcall.io.count = 0;
		return;
	}

	count = r->ifcall.io.count;
	if(max && (offset + count > max))
		count = max - offset;

	*len = offset + count;
	if(max == 0)
		*buf = erealloc(*buf, *len + 1);
	p = *buf;

	memcpy(p+offset, r->ifcall.io.data, count);
	r->ofcall.io.count = count;
	p[offset+count] = '\0';
}

/**
 * Ensure that the data member of 'r' is null terminated,
 * removing any new line from its end.
 */
void
ixp_srv_data2cstring(Ixp9Req *r) {
	char *p, *q;
	uint i;

	i = r->ifcall.io.count;
	p = r->ifcall.io.data;
	if(i && p[i - 1] == '\n')
		i--;
	q = memchr(p, '\0', i);
	if(q)
		i = q - p;

	p = erealloc(r->ifcall.io.data, i+1);
	p[i] = '\0';
	r->ifcall.io.data = p;
}

char*
ixp_srv_writectl(Ixp9Req *r, char* (*fn)(void*, IxpMsg*)) {
	char *err, *s, *p, c;
	IxpFileId *f;
	IxpMsg m;

	f = r->fid->aux;

	ixp_srv_data2cstring(r);
	s = r->ifcall.io.data;

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

		m = ixp_message(s, p-s, 0);
		s = fn(f->p, &m);
		if(s)
			err = s;
		s = p + 1;
	}
	return err;
}

void
ixp_pending_respond(Ixp9Req *r) {
	IxpFileId *f;
	IxpPLink *p;
	IxpRLink *rl;
	IxpQueue *q;

	f = r->fid->aux;
	p = f->p;
	assert(f->pending);
	if(p->queue) {
		q = p->queue;
		p->queue = q->link;
		r->ofcall.io.data = q->dat;
		r->ofcall.io.count = q->len;
		if(r->aux) {
			rl = r->aux;
			rl->next->prev = rl->prev;
			rl->prev->next = rl->next;
			free(rl);
		}
		respond(r, nil);
		free(q);
	}else {
		rl = emallocz(sizeof *rl);
		rl->req = r;
		rl->next = &p->pending->req;
		rl->prev = rl->next->prev;
		rl->next->prev = rl;
		rl->prev->next = rl;
		r->aux = rl;
	}
}

void
ixp_pending_write(IxpPending *p, char *dat, long n) {
	IxpRLink rl;
	IxpQueue **qp, *q;
	IxpPLink *pp;
	IxpRLink *rp;

	if(n == 0)
		return;

	if(p->req.next == nil) {
		p->req.next = &p->req;
		p->req.prev = &p->req;
		p->fids.prev = &p->fids;
		p->fids.next = &p->fids;
	}

	for(pp=p->fids.next; pp != &p->fids; pp=pp->next) {
		for(qp=&pp->queue; *qp; qp=&qp[0]->link)
			;
		q = emallocz(sizeof *q);
		q->dat = emalloc(n);
		memcpy(q->dat, dat, n);
		q->len = n;
		*qp = q;
	}
	rl.next = &rl;
	rl.prev = &rl;
	if(p->req.next != &p->req) {
		rl.next = p->req.next;
		rl.prev = p->req.prev;
		p->req.prev = &p->req;
		p->req.next = &p->req;
	}
	rl.prev->next = &rl;
	rl.next->prev = &rl;
	while((rp = rl.next) != &rl)
		ixp_pending_respond(rp->req);
}

void
ixp_pending_pushfid(IxpPending *p, IxpFid *f) {
	IxpPLink *pl;
	IxpFileId *fi;

	if(p->req.next == nil) {
		p->req.next = &p->req;
		p->req.prev = &p->req;
		p->fids.prev = &p->fids;
		p->fids.next = &p->fids;
	}

	fi = f->aux;
	pl = emallocz(sizeof *pl);
	pl->fid = f;
	pl->pending = p;
	pl->next = &p->fids;
	pl->prev = pl->next->prev;
	pl->next->prev = pl;
	pl->prev->next = pl;
	fi->pending = true;
	fi->p = pl;
}

void
ixp_pending_flush(Ixp9Req *r) {
	Ixp9Req *or;
	IxpFileId *f;
	IxpRLink *rl;

	or = r->oldreq;
	f = or->fid->aux;
	if(f->pending) {
		rl = or->aux;
		if(rl) {
			rl->prev->next = rl->next;
			rl->next->prev = rl->prev;
			free(rl);
		}
	}
}

bool
ixp_pending_clunk(Ixp9Req *r) {
	IxpPending *p;
	IxpFileId *f;
	IxpPLink *pl;
	IxpRLink *rl;
	IxpQueue *qu;
	bool more;

	f = r->fid->aux;
	pl = f->p;

	p = pl->pending;
	for(rl=p->req.next; rl != &p->req; rl=rl->next)
		if(rl->req->fid == pl->fid) {
			respond(r, "fid in use");
			return true;
		}

	pl->prev->next = pl->next;
	pl->next->prev = pl->prev;

	while((qu = pl->queue)) {
		pl->queue = qu->link;
		free(qu->dat);
		free(qu);
	}
	more = (pl->pending->fids.next == &pl->pending->fids);
	free(pl);
	respond(r, nil);
	return more;
}

bool
ixp_srv_verifyfile(IxpFileId *f, IxpLookupFn lookup) {
	IxpFileId *nf;
	int ret;

	if(!f->next)
		return true;

	ret = false;
	if(ixp_srv_verifyfile(f->next, lookup)) {
		nf = lookup(f->next, f->tab.name);
		if(nf) {
			if(!nf->volatil || nf->p == f->p)
				ret = true;
			ixp_srv_freefile(nf);
		}
	}
	return ret;
}

void
ixp_srv_readdir(Ixp9Req *r, IxpLookupFn lookup, void (*dostat)(IxpStat*, IxpFileId*)) {
	IxpMsg m;
	IxpFileId *f, *tf;
	IxpStat s;
	char *buf;
	ulong size, n;
	uvlong offset;

	f = r->fid->aux;

	size = r->ifcall.io.count;
	if(size > r->fid->iounit)
		size = r->fid->iounit;
	buf = emallocz(size);
	m = ixp_message(buf, size, MsgPack);

	f = lookup(f, nil);
	tf = f;
	/* Note: The first f is ".", so we skip it. */
	offset = 0;
	for(f=f->next; f; f=f->next) {
		dostat(&s, f);
		n = ixp_sizeof_stat(&s);
		if(offset >= r->ifcall.io.offset) {
			if(size < n)
				break;
			ixp_pstat(&m, &s);
			size -= n;
		}
		offset += n;
	}
	while((f = tf)) {
		tf=tf->next;
		ixp_srv_freefile(f);
	}
	r->ofcall.io.count = m.pos - m.data;
	r->ofcall.io.data = m.data;
	respond(r, nil);
}

void
ixp_srv_walkandclone(Ixp9Req *r, IxpLookupFn lookup) {
	IxpFileId *f, *nf;
	int i;

	f = r->fid->aux;
	ixp_srv_clonefiles(f);
	for(i=0; i < r->ifcall.twalk.nwname; i++) {
		if(!strcmp(r->ifcall.twalk.wname[i], "..")) {
			if(f->next) {
				nf=f;
				f=f->next;
				ixp_srv_freefile(nf);
			}
		}else{
			nf = lookup(f, r->ifcall.twalk.wname[i]);
			if(!nf)
				break;
			assert(!nf->next);
			if(strcmp(r->ifcall.twalk.wname[i], ".")) {
				nf->next = f;
				f = nf;
			}
		}
		r->ofcall.rwalk.wqid[i].type = f->tab.qtype;
		r->ofcall.rwalk.wqid[i].path = QID(f->tab.type, f->id);
	}
	/* There should be a way to do this on freefid() */
	if(i < r->ifcall.twalk.nwname) {
		while((nf = f)) {
			f=f->next;
			ixp_srv_freefile(nf);
		}
		respond(r, Enofile);
		return;
	}
	/* Remove refs for r->fid if no new fid */
	if(r->ifcall.hdr.fid == r->ifcall.twalk.newfid) {
		nf = r->fid->aux;
		r->fid->aux = f;
		while((f = nf)) {
			nf = nf->next;
			ixp_srv_freefile(f);
		}
	}else
		r->newfid->aux = f;
	r->ofcall.rwalk.nwqid = i;
	respond(r, nil);
}

