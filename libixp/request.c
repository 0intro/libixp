/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "ixp_local.h"

static void handlereq(Ixp9Req *r);

static void
_printfcall(Fcall *f) {
	USED(f);
}
void (*ixp_printfcall)(Fcall*) = _printfcall;

static int
min(int a, int b) {
	if(a < b)
		return a;
	return b;
}

static char
	Eduptag[] = "tag in use",
	Edupfid[] = "fid in use",
	Enofunc[] = "function not implemented",
	Eopen[] = "fid is already open",
	Enofile[] = "file does not exist",
	Enoread[] = "file not open for reading",
	Enofid[] = "fid does not exist",
	Enotag[] = "tag does not exist",
	Enotdir[] = "not a directory",
	Eintr[] = "interrupted",
	Eisdir[] = "cannot perform operation on a directory";

enum {
	TAG_BUCKETS = 61,
	FID_BUCKETS = 61,
};

struct Ixp9Conn {
	Intmap		tagmap;
	Intmap		fidmap;
	void		*taghash[TAG_BUCKETS];
	void		*fidhash[FID_BUCKETS];
	Ixp9Srv		*srv;
	IxpConn		*conn;
	IxpMutex	rlock, wlock;
	IxpMsg		rmsg;
	IxpMsg		wmsg;
	int		ref;
};

static void
decref_p9conn(Ixp9Conn *pc) {
	thread->lock(&pc->wlock);
	if(--pc->ref > 0) {
		thread->unlock(&pc->wlock);
		return;
	}
	thread->unlock(&pc->wlock);

	assert(pc->conn == nil);

	thread->mdestroy(&pc->rlock);
	thread->mdestroy(&pc->wlock);

	freemap(&pc->tagmap, nil);
	freemap(&pc->fidmap, nil);

	free(pc->rmsg.data);
	free(pc->wmsg.data);
	free(pc);
}

static void*
createfid(Intmap *map, int fid, Ixp9Conn *pc) {
	Fid *f;

	f = emallocz(sizeof *f);
	pc->ref++;
	f->conn = pc;
	f->fid = fid;
	f->omode = -1;
	f->map = map;
	if(caninsertkey(map, fid, f))
		return f;
	free(f);
	return nil;
}

static int
destroyfid(Ixp9Conn *pc, ulong fid) {
	Fid *f;

	f = deletekey(&pc->fidmap, fid);
	if(f == nil)
		return 0;

	if(pc->srv->freefid)
		pc->srv->freefid(f);

	decref_p9conn(pc);
	free(f);
	return 1;
}

static void
handlefcall(IxpConn *c) {
	Fcall fcall = {0};
	Ixp9Conn *pc;
	Ixp9Req *req;

	pc = c->aux;

	thread->lock(&pc->rlock);
	if(ixp_recvmsg(c->fd, &pc->rmsg) == 0)
		goto Fail;
	if(ixp_msg2fcall(&pc->rmsg, &fcall) == 0)
		goto Fail;
	thread->unlock(&pc->rlock);

	req = emallocz(sizeof *req);
	pc->ref++;
	req->conn = pc;
	req->srv = pc->srv;
	req->ifcall = fcall;
	pc->conn = c;

	if(caninsertkey(&pc->tagmap, fcall.hdr.tag, req) == 0) {
		respond(req, Eduptag);
		return;
	}

	handlereq(req);
	return;

Fail:
	thread->unlock(&pc->rlock);
	ixp_hangup(c);
	return;
}

static void
handlereq(Ixp9Req *r) {
	Ixp9Conn *pc;
	Ixp9Srv *srv;

	pc = r->conn;
	srv = pc->srv;

	ixp_printfcall(&r->ifcall);

	switch(r->ifcall.hdr.type) {
	default:
		respond(r, Enofunc);
		break;
	case TVersion:
		if(!strcmp(r->ifcall.version.version, "9P"))
			r->ofcall.version.version = "9P";
		else if(!strcmp(r->ifcall.version.version, "9P2000"))
			r->ofcall.version.version = "9P2000";
		else
			r->ofcall.version.version = "unknown";
		r->ofcall.version.msize = r->ifcall.version.msize;
		respond(r, nil);
		break;
	case TAttach:
		if(!(r->fid = createfid(&pc->fidmap, r->ifcall.hdr.fid, pc))) {
			respond(r, Edupfid);
			return;
		}
		/* attach is a required function */
		srv->attach(r);
		break;
	case TClunk:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.hdr.fid))) {
			respond(r, Enofid);
			return;
		}
		if(!srv->clunk) {
			respond(r, nil);
			return;
		}
		srv->clunk(r);
		break;
	case TFlush:
		if(!(r->oldreq = lookupkey(&pc->tagmap, r->ifcall.tflush.oldtag))) {
			respond(r, Enotag);
			return;
		}
		if(!srv->flush) {
			respond(r, Enofunc);
			return;
		}
		srv->flush(r);
		break;
	case TCreate:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.hdr.fid))) {
			respond(r, Enofid);
			return;
		}
		if(r->fid->omode != -1) {
			respond(r, Eopen);
			return;
		}
		if(!(r->fid->qid.type&QTDIR)) {
			respond(r, Enotdir);
			return;
		}
		if(!pc->srv->create) {
			respond(r, Enofunc);
			return;
		}
		pc->srv->create(r);
		break;
	case TOpen:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.hdr.fid))) {
			respond(r, Enofid);
			return;
		}
		if((r->fid->qid.type&QTDIR) && (r->ifcall.topen.mode|P9_ORCLOSE) != (P9_OREAD|P9_ORCLOSE)) {
			respond(r, Eisdir);
			return;
		}
		r->ofcall.ropen.qid = r->fid->qid;
		if(!pc->srv->open) {
			respond(r, Enofunc);
			return;
		}
		pc->srv->open(r);
		break;
	case TRead:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.hdr.fid))) {
			respond(r, Enofid);
			return;
		}
		if(r->fid->omode == -1 || r->fid->omode == P9_OWRITE) {
			respond(r, Enoread);
			return;
		}
		if(!pc->srv->read) {
			respond(r, Enofunc);
			return;
		}
		pc->srv->read(r);
		break;
	case TRemove:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.hdr.fid))) {
			respond(r, Enofid);
			return;
		}
		if(!pc->srv->remove) {
			respond(r, Enofunc);
			return;
		}
		pc->srv->remove(r);
		break;
	case TStat:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.hdr.fid))) {
			respond(r, Enofid);
			return;
		}
		if(!pc->srv->stat) {
			respond(r, Enofunc);
			return;
		}
		pc->srv->stat(r);
		break;
	case TWalk:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.hdr.fid))) {
			respond(r, Enofid);
			return;
		}
		if(r->fid->omode != -1) {
			respond(r, "cannot walk from an open fid");
			return;
		}
		if(r->ifcall.twalk.nwname && !(r->fid->qid.type&QTDIR)) {
			respond(r, Enotdir);
			return;
		}
		if((r->ifcall.hdr.fid != r->ifcall.twalk.newfid)) {
			if(!(r->newfid = createfid(&pc->fidmap, r->ifcall.twalk.newfid, pc))) {
				respond(r, Edupfid);
				return;
			}
		}else
			r->newfid = r->fid;
		if(!pc->srv->walk) {
			respond(r, Enofunc);
			return;
		}
		pc->srv->walk(r);
		break;
	case TWrite:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.hdr.fid))) {
			respond(r, Enofid);
			return;
		}
		if((r->fid->omode&3) != P9_OWRITE && (r->fid->omode&3) != P9_ORDWR) {
			respond(r, "write on fid not opened for writing");
			return;
		}
		if(!pc->srv->write) {
			respond(r, Enofunc);
			return;
		}
		pc->srv->write(r);
		break;
	case TWStat:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.hdr.fid))) {
			respond(r, Enofid);
			return;
		}
		if((ushort)~r->ifcall.twstat.stat.type) {
			respond(r, "wstat of type");
			return;
		}
		if((uint)~r->ifcall.twstat.stat.dev) {
			respond(r, "wstat of dev");
			return;
		}
		if((uchar)~r->ifcall.twstat.stat.qid.type || (ulong)~r->ifcall.twstat.stat.qid.version || (uvlong)~r->ifcall.twstat.stat.qid.path) {
			respond(r, "wstat of qid");
			return;
		}
		if(r->ifcall.twstat.stat.muid && r->ifcall.twstat.stat.muid[0]) {
			respond(r, "wstat of muid");
			return;
		}
		if((ulong)~r->ifcall.twstat.stat.mode && ((r->ifcall.twstat.stat.mode&DMDIR)>>24) != r->fid->qid.type&QTDIR) {
			respond(r, "wstat on DMDIR bit");
			return;
		}
		pc->srv->wstat(r);
		break;
	/* Still to be implemented: auth */
	}
}

void
respond(Ixp9Req *r, const char *error) {
	Ixp9Conn *pc;
	int msize;

	pc = r->conn;

	switch(r->ifcall.hdr.type) {
	default:
		if(!error)
			assert(!"Respond called on unsupported fcall type");
		break;
	case TVersion:
		assert(error == nil);
		free(r->ifcall.version.version);

		thread->lock(&pc->rlock);
		thread->lock(&pc->wlock);
		msize = min(r->ofcall.version.msize, IXP_MAX_MSG);
		pc->rmsg.data = erealloc(pc->rmsg.data, msize);
		pc->wmsg.data = erealloc(pc->wmsg.data, msize);
		pc->rmsg.size = msize;
		pc->wmsg.size = msize;
		thread->unlock(&pc->wlock);
		thread->unlock(&pc->rlock);
		r->ofcall.version.msize = msize;
		break;
	case TAttach:
		if(error)
			destroyfid(pc, r->fid->fid);
		free(r->ifcall.tattach.uname);
		free(r->ifcall.tattach.aname);
		break;
	case TOpen:
	case TCreate:
		if(!error) {
			r->ofcall.ropen.iounit = pc->rmsg.size - 24;
			r->fid->iounit = r->ofcall.ropen.iounit;
			r->fid->omode = r->ifcall.topen.mode;
			r->fid->qid = r->ofcall.ropen.qid;
		}
		free(r->ifcall.tcreate.name);
		break;
	case TWalk:
		if(error || r->ofcall.rwalk.nwqid < r->ifcall.twalk.nwname) {
			if(r->ifcall.hdr.fid != r->ifcall.twalk.newfid && r->newfid)
				destroyfid(pc, r->newfid->fid);
			if(!error && r->ofcall.rwalk.nwqid == 0)
				error = Enofile;
		}else{
			if(r->ofcall.rwalk.nwqid == 0)
				r->newfid->qid = r->fid->qid;
			else
				r->newfid->qid = r->ofcall.rwalk.wqid[r->ofcall.rwalk.nwqid-1];
		}
		free(*r->ifcall.twalk.wname);
		break;
	case TWrite:
		free(r->ifcall.twrite.data);
		break;
	case TRemove:
		if(r->fid)
			destroyfid(pc, r->fid->fid);
		break;
	case TClunk:
		if(r->fid)
			destroyfid(pc, r->fid->fid);
		break;
	case TFlush:
		if((r->oldreq = lookupkey(&pc->tagmap, r->ifcall.tflush.oldtag)))
			respond(r->oldreq, Eintr);
		break;
	case TWStat:
		ixp_freestat(&r->ifcall.twstat.stat);
		break;
	case TRead:
	case TStat:
		break;		
	/* Still to be implemented: auth */
	}

	r->ofcall.hdr.tag = r->ifcall.hdr.tag;

	if(error == nil)
		r->ofcall.hdr.type = r->ifcall.hdr.type + 1;
	else {
		r->ofcall.hdr.type = RError;
		r->ofcall.error.ename = (char*)error;
	}

	deletekey(&pc->tagmap, r->ifcall.hdr.tag);;

	if(pc->conn) {
		thread->lock(&pc->wlock);
		msize = ixp_fcall2msg(&pc->wmsg, &r->ofcall);
		if(ixp_sendmsg(pc->conn->fd, &pc->wmsg) != msize)
			ixp_hangup(pc->conn);
		thread->unlock(&pc->wlock);
	}

	switch(r->ofcall.hdr.type) {
	case RStat:
		free(r->ofcall.rstat.stat);
		break;
	case RRead:
		free(r->ofcall.rread.data);
		break;
	}
	free(r);
	decref_p9conn(pc);
}

/* Flush a pending request */
static void
voidrequest(void *t) {
	Ixp9Req *r, *tr;
	Ixp9Conn *pc;

	r = t;
	pc = r->conn;
	pc->ref++;

	tr = emallocz(sizeof *tr);
	tr->ifcall.hdr.type = TFlush;
	tr->ifcall.hdr.tag = IXP_NOTAG;
	tr->ifcall.tflush.oldtag = r->ifcall.hdr.tag;
	tr->conn = pc;
	handlereq(tr);
}

/* Clunk an open Fid */
static void
voidfid(void *t) {
	Ixp9Conn *pc;
	Ixp9Req *tr;
	Fid *f;

	f = t;
	pc = f->conn;
	pc->ref++;

	tr = emallocz(sizeof *tr);
	tr->ifcall.hdr.type = TClunk;
	tr->ifcall.hdr.tag = IXP_NOTAG;
	tr->ifcall.hdr.fid = f->fid;
	tr->fid = f;
	tr->conn = pc;
	handlereq(tr);
}

static void
cleanupconn(IxpConn *c) {
	Ixp9Conn *pc;

	pc = c->aux;
	pc->conn = nil;
	if(pc->ref > 1) {
		execmap(&pc->tagmap, voidrequest);
		execmap(&pc->fidmap, voidfid);
	}
	decref_p9conn(pc);
}

/* Handle incoming 9P connections */
void
serve_9pcon(IxpConn *c) {
	Ixp9Conn *pc;
	int fd;

	fd = accept(c->fd, nil, nil);
	if(fd < 0)
		return;

	pc = emallocz(sizeof *pc);
	pc->ref++;
	pc->srv = c->aux;
	pc->rmsg.size = 1024;
	pc->wmsg.size = 1024;
	pc->rmsg.data = emalloc(pc->rmsg.size);
	pc->wmsg.data = emalloc(pc->wmsg.size);

	initmap(&pc->tagmap, TAG_BUCKETS, &pc->taghash);
	initmap(&pc->fidmap, FID_BUCKETS, &pc->fidhash);
	thread->initmutex(&pc->rlock);
	thread->initmutex(&pc->wlock);

	ixp_listen(c->srv, fd, pc, handlefcall, cleanupconn);
}
