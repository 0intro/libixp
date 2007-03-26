/* Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "ixp.h"

static void handlereq(Ixp9Req *r);

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
	Ebotch[] = "9P protocol botch",
	Enofile[] = "file does not exist",
	Enofid[] = "fid does not exist",
	Enotag[] = "tag does not exist",
	Enotdir[] = "not a directory",
	Eintr[] = "interrupted",
	Eisdir[] = "cannot perform operation on a directory";

enum {
	TAG_BUCKETS = 64,
	FID_BUCKETS = 64,
};

struct Ixp9Conn {
	Intmap	tagmap;
	void	*taghash[TAG_BUCKETS];
	Intmap	fidmap;
	void	*fidhash[FID_BUCKETS];
	Ixp9Srv	*srv;
	IxpConn	*conn;
	Message	msg;
	uint ref;
};

static void
free_p9conn(Ixp9Conn *pc) {
	free(pc->msg.data);
	free(pc);
}

static void *
createfid(Intmap *map, int fid, Ixp9Conn *pc) {
	Fid *f;

	pc->ref++;
	f = ixp_emallocz(sizeof(Fid));
	f->fid = fid;
	f->omode = -1;
	f->map = map;
	f->conn = pc;
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

	pc->ref--;
	free(f);
	return 1;
}

static void
handlefcall(IxpConn *c) {
	Fcall fcall = {0};
	Ixp9Conn *pc;
	Ixp9Req *req;

	pc = c->aux;
	errstr = nil;

	if(ixp_recvmsg(c->fd, &pc->msg) == 0)
		goto Fail;
	if(ixp_msg2fcall(&pc->msg, &fcall) == 0)
		goto Fail;

	req = ixp_emallocz(sizeof(Ixp9Req));
	req->conn = pc;
	req->ifcall = fcall;
	pc->ref++;
	pc->conn = c;
	if(caninsertkey(&pc->tagmap, fcall.tag, req) == 0) {
		respond(req, Eduptag);
		return;
	}
	handlereq(req);
	return;

Fail:
	ixp_hangup(c);
	return;
}

static void
handlereq(Ixp9Req *r) {
	Ixp9Conn *pc;
	Ixp9Srv *srv;

	pc = r->conn;
	srv = pc->srv;

	switch(r->ifcall.type) {
	default:
		respond(r, Enofunc);
		break;
	case TVersion:
		if(!strcmp(r->ifcall.version, "9P"))
			r->ofcall.version = "9P";
		else if(!strcmp(r->ifcall.version, "9P2000"))
			r->ofcall.version = "9P2000";
		else
			r->ofcall.version = "unknown";
		r->ofcall.msize = r->ifcall.msize;
		respond(r, nil);
		break;
	case TAttach:
		if(!(r->fid = createfid(&pc->fidmap, r->ifcall.fid, pc))) {
			respond(r, Edupfid);
			return;
		}
		/* attach is a required function */
		srv->attach(r);
		break;
	case TClunk:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.fid))) {
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
		if(!(r->oldreq = lookupkey(&pc->tagmap, r->ifcall.oldtag))) {
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
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.fid))) {
			respond(r, Enofid);
			return;
		}
		if(r->fid->omode != -1) {
			respond(r, Ebotch);
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
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.fid))) {
			respond(r, Enofid);
			return;
		}
		if((r->fid->qid.type&QTDIR) && (r->ifcall.mode|P9_ORCLOSE) != (P9_OREAD|P9_ORCLOSE)) {
			respond(r, Eisdir);
			return;
		}
		r->ofcall.qid = r->fid->qid;
		if(!pc->srv->open) {
			respond(r, Enofunc);
			return;
		}
		pc->srv->open(r);
		break;
	case TRead:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.fid))) {
			respond(r, Enofid);
			return;
		}
		if(r->fid->omode == -1 || r->fid->omode == P9_OWRITE) {
			respond(r, Ebotch);
			return;
		}
		if(!pc->srv->read) {
			respond(r, Enofunc);
			return;
		}
		pc->srv->read(r);
		break;
	case TRemove:
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.fid))) {
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
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.fid))) {
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
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.fid))) {
			respond(r, Enofid);
			return;
		}
		if(r->fid->omode != -1) {
			respond(r, "cannot walk from an open fid");
			return;
		}
		if(r->ifcall.nwname && !(r->fid->qid.type&QTDIR)) {
			respond(r, Enotdir);
			return;
		}
		if((r->ifcall.fid != r->ifcall.newfid)) {
			if(!(r->newfid = createfid(&pc->fidmap, r->ifcall.newfid, pc))) {
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
		if(!(r->fid = lookupkey(&pc->fidmap, r->ifcall.fid))) {
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
	/* Still to be implemented: flush, wstat, auth */
	}
}

void
respond(Ixp9Req *r, char *error) {
	Ixp9Conn *pc;
	int msize;

	pc = r->conn;

	switch(r->ifcall.type) {
	default:
		if(!error)
			assert(!"Respond called on unsupported fcall type");
		break;
	case TVersion:
		assert(error == nil);
		free(r->ifcall.version);

		pc->msg.size = min(r->ofcall.msize, IXP_MAX_MSG);
		pc->msg.data = ixp_erealloc(pc->msg.data, pc->msg.size);
		r->ofcall.msize = pc->msg.size;
		break;
	case TAttach:
		if(error)
			destroyfid(pc, r->fid->fid);
		free(r->ifcall.uname);
		free(r->ifcall.aname);
		break;
	case TOpen:
	case TCreate:
		if(!error) {
			r->fid->omode = r->ifcall.mode;
			r->fid->qid = r->ofcall.qid;
		}
		free(r->ifcall.name);
		r->ofcall.iounit = pc->msg.size - sizeof(ulong);
		break;
	case TWalk:
		if(error || r->ofcall.nwqid < r->ifcall.nwname) {
			if(r->ifcall.fid != r->ifcall.newfid && r->newfid)
				destroyfid(pc, r->newfid->fid);
			if(!error && r->ofcall.nwqid == 0)
				error = Enofile;
		}else{
			if(r->ofcall.nwqid == 0)
				r->newfid->qid = r->fid->qid;
			else
				r->newfid->qid = r->ofcall.wqid[r->ofcall.nwqid-1];
		}
		free(*r->ifcall.wname);
		break;
	case TWrite:
		free(r->ifcall.data);
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
		if((r->oldreq = lookupkey(&pc->tagmap, r->ifcall.oldtag)))
			respond(r->oldreq, Eintr);
		break;
	case TRead:
	case TStat:
		break;
	/* Still to be implemented: flush, wstat, auth */
	}

	r->ofcall.tag = r->ifcall.tag;

	if(error == nil)
		r->ofcall.type = r->ifcall.type + 1;
	else {
		r->ofcall.type = RError;
		r->ofcall.ename = error;
	}

	deletekey(&pc->tagmap, r->ifcall.tag);;

	if(pc->conn) {
		msize = ixp_fcall2msg(&pc->msg, &r->ofcall);
		if(ixp_sendmsg(pc->conn->fd, &pc->msg) != msize)
			ixp_hangup(pc->conn);
	}

	switch(r->ofcall.type) {
	case RStat:
		free(r->ofcall.stat);
		break;
	case RRead:
		free(r->ofcall.data);
		break;
	}
	free(r);

	pc->ref--;
	if(!pc->conn && pc->ref == 0)
		free_p9conn(pc);
}

/* Flush a pending request */
static void
voidrequest(void *t) {
	Ixp9Req *r, *tr;
	Ixp9Conn *pc;

	r = t;
	pc = r->conn;
	pc->ref++;

	tr = ixp_emallocz(sizeof(Ixp9Req));
	tr->ifcall.type = TFlush;
	tr->ifcall.tag = IXP_NOTAG;
	tr->ifcall.oldtag = r->ifcall.tag;
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

	tr = ixp_emallocz(sizeof(Ixp9Req));
	tr->ifcall.type = TClunk;
	tr->ifcall.tag = IXP_NOTAG;
	tr->ifcall.fid = f->fid;
	tr->fid = f;
	tr->conn = pc;
	handlereq(tr);
}

#if 0
static void
p9conn_incref(void *r) {
	Ixp9Conn *pc;
	
	pc = *(Ixp9Conn **)r;
	pc->ref++;
}
#endif

static void
cleanupconn(IxpConn *c) {
	Ixp9Conn *pc;

	pc = c->aux;
	pc->conn = nil;
	pc->ref++;
	if(pc->ref > 1) {
		execmap(&pc->tagmap, voidrequest);
		execmap(&pc->fidmap, voidfid);
	}
	if(--pc->ref == 0)
		free_p9conn(pc);
}

/* Handle incoming 9P connections */
void
serve_9pcon(IxpConn *c) {
	Ixp9Conn *pc;
	int fd;

	fd = accept(c->fd, nil, nil);
	if(fd < 0)
		return;

	pc = ixp_emallocz(sizeof(Ixp9Conn));
	pc->srv = c->aux;
	pc->msg.size = 1024;
	pc->msg.data = ixp_emalloc(pc->msg.size);
	initmap(&pc->tagmap, TAG_BUCKETS, &pc->taghash);
	initmap(&pc->fidmap, FID_BUCKETS, &pc->fidhash);

	ixp_listen(c->srv, fd, pc, handlefcall, cleanupconn);
}
