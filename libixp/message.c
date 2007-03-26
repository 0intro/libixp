/* Copyright ©2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ixp.h"

enum {
	SByte = 1,
	SWord = 2,
	SDWord = 4,
	SQWord = 8,
};

#define SString(s) (SWord + strlen(s))
enum {
	SQid = SByte + SDWord + SQWord,
};

Message
ixp_message(uchar *data, uint length, uint mode) {
	Message m;

	m.data = data;
	m.pos = data;
	m.end = data + length;
	m.size = length;
	m.mode = mode;
	return m;
}

void
ixp_freestat(Stat *s) {
	free(s->name);
	free(s->uid);
	free(s->gid);
	free(s->muid);
	s->name = s->uid = s->gid = s->muid = nil;
}

void
ixp_freefcall(Fcall *fcall) {
	switch(fcall->type) {
	case RStat:
		free(fcall->stat);
		fcall->stat = nil;
		break;
	case RRead:
		free(fcall->data);
		fcall->data = nil;
		break;
	case RVersion:
		free(fcall->version);
		fcall->version = nil;
		break;
	case RError:
		free(fcall->ename);
		fcall->ename = nil;
		break;
	}
}

ushort
ixp_sizeof_stat(Stat * stat) {
	return SWord /* size */
		+ SWord /* type */
		+ SDWord /* dev */
		+ SQid /* qid */
		+ 3 * SDWord /* mode, atime, mtime */
		+ SQWord /* length */
		+ SString(stat->name)
		+ SString(stat->uid)
		+ SString(stat->gid)
		+ SString(stat->muid);
}

void
ixp_pfcall(Message *msg, Fcall *fcall) {
	ixp_pu8(msg, &fcall->type);
	ixp_pu16(msg, &fcall->tag);

	switch (fcall->type) {
	case TVersion:
	case RVersion:
		ixp_pu32(msg, &fcall->msize);
		ixp_pstring(msg, &fcall->version);
		break;
	case TAuth:
		ixp_pu32(msg, &fcall->afid);
		ixp_pstring(msg, &fcall->uname);
		ixp_pstring(msg, &fcall->aname);
		break;
	case RAuth:
		ixp_pqid(msg, &fcall->aqid);
		break;
	case RAttach:
		ixp_pqid(msg, &fcall->qid);
		break;
	case TAttach:
		ixp_pu32(msg, &fcall->fid);
		ixp_pu32(msg, &fcall->afid);
		ixp_pstring(msg, &fcall->uname);
		ixp_pstring(msg, &fcall->aname);
		break;
	case RError:
		ixp_pstring(msg, &fcall->ename);
		break;
	case TFlush:
		ixp_pu16(msg, &fcall->oldtag);
		break;
	case TWalk:
		ixp_pu32(msg, &fcall->fid);
		ixp_pu32(msg, &fcall->newfid);
		ixp_pstrings(msg, &fcall->nwname, fcall->wname);
		break;
	case RWalk:
		ixp_pqids(msg, &fcall->nwqid, fcall->wqid);
		break;
	case TOpen:
		ixp_pu32(msg, &fcall->fid);
		ixp_pu8(msg, &fcall->mode);
		break;
	case ROpen:
	case RCreate:
		ixp_pqid(msg, &fcall->qid);
		ixp_pu32(msg, &fcall->iounit);
		break;
	case TCreate:
		ixp_pu32(msg, &fcall->fid);
		ixp_pstring(msg, &fcall->name);
		ixp_pu32(msg, &fcall->perm);
		ixp_pu8(msg, &fcall->mode);
		break;
	case TRead:
		ixp_pu32(msg, &fcall->fid);
		ixp_pu64(msg, &fcall->offset);
		ixp_pu32(msg, &fcall->count);
		break;
	case RRead:
		ixp_pu32(msg, &fcall->count);
		ixp_pdata(msg, &fcall->data, fcall->count);
		break;
	case TWrite:
		ixp_pu32(msg, &fcall->fid);
		ixp_pu64(msg, &fcall->offset);
		ixp_pu32(msg, &fcall->count);
		ixp_pdata(msg, &fcall->data, fcall->count);
		break;
	case RWrite:
		ixp_pu32(msg, &fcall->count);
		break;
	case TClunk:
	case TRemove:
	case TStat:
		ixp_pu32(msg, &fcall->fid);
		break;
	case RStat:
		ixp_pu16(msg, &fcall->nstat);
		ixp_pdata(msg, (char**)&fcall->stat, fcall->nstat);
		break;
	case TWStat:
		ixp_pu32(msg, &fcall->fid);
		ixp_pu16(msg, &fcall->nstat);
		ixp_pdata(msg, (char**)&fcall->stat, fcall->nstat);
		break;
	}
}

uint
ixp_fcall2msg(Message *msg, Fcall *fcall) {
	int size;

	msg->end = msg->data + msg->size;
	msg->pos = msg->data + SDWord;
	msg->mode = MsgPack;
	ixp_pfcall(msg, fcall);

	if(msg->pos > msg->end)
		return 0;

	msg->end = msg->pos;
	size = msg->end - msg->data;

	msg->pos = msg->data;
	ixp_pu32(msg, &size);

	msg->pos = msg->data;
	return size;
}

uint
ixp_msg2fcall(Message *msg, Fcall *fcall) {
	msg->pos = msg->data + SDWord;
	msg->mode = MsgUnpack;
	ixp_pfcall(msg, fcall);

	if(msg->pos > msg->end)
		return 0;

	return msg->pos - msg->data;
}
