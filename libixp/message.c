/* Copyright ©2007-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ixp_local.h"

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

IxpMsg
ixp_message(char *data, uint length, uint mode) {
	IxpMsg m;

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
	switch(fcall->hdr.type) {
	case RStat:
		free(fcall->rstat.stat);
		fcall->rstat.stat = nil;
		break;
	case RRead:
		free(fcall->rread.data);
		fcall->rread.data = nil;
		break;
	case RVersion:
		free(fcall->version.version);
		fcall->version.version = nil;
		break;
	case RError:
		free(fcall->error.ename);
		fcall->error.ename = nil;
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
ixp_pfcall(IxpMsg *msg, Fcall *fcall) {
	ixp_pu8(msg, &fcall->hdr.type);
	ixp_pu16(msg, &fcall->hdr.tag);

	switch (fcall->hdr.type) {
	case TVersion:
	case RVersion:
		ixp_pu32(msg, &fcall->version.msize);
		ixp_pstring(msg, &fcall->version.version);
		break;
	case TAuth:
		ixp_pu32(msg, &fcall->tauth.afid);
		ixp_pstring(msg, &fcall->tauth.uname);
		ixp_pstring(msg, &fcall->tauth.aname);
		break;
	case RAuth:
		ixp_pqid(msg, &fcall->rauth.aqid);
		break;
	case RAttach:
		ixp_pqid(msg, &fcall->rattach.qid);
		break;
	case TAttach:
		ixp_pu32(msg, &fcall->hdr.fid);
		ixp_pu32(msg, &fcall->tattach.afid);
		ixp_pstring(msg, &fcall->tattach.uname);
		ixp_pstring(msg, &fcall->tattach.aname);
		break;
	case RError:
		ixp_pstring(msg, &fcall->error.ename);
		break;
	case TFlush:
		ixp_pu16(msg, &fcall->tflush.oldtag);
		break;
	case TWalk:
		ixp_pu32(msg, &fcall->hdr.fid);
		ixp_pu32(msg, &fcall->twalk.newfid);
		ixp_pstrings(msg, &fcall->twalk.nwname, fcall->twalk.wname);
		break;
	case RWalk:
		ixp_pqids(msg, &fcall->rwalk.nwqid, fcall->rwalk.wqid);
		break;
	case TOpen:
		ixp_pu32(msg, &fcall->hdr.fid);
		ixp_pu8(msg, &fcall->topen.mode);
		break;
	case ROpen:
	case RCreate:
		ixp_pqid(msg, &fcall->ropen.qid);
		ixp_pu32(msg, &fcall->ropen.iounit);
		break;
	case TCreate:
		ixp_pu32(msg, &fcall->hdr.fid);
		ixp_pstring(msg, &fcall->tcreate.name);
		ixp_pu32(msg, &fcall->tcreate.perm);
		ixp_pu8(msg, &fcall->tcreate.mode);
		break;
	case TRead:
		ixp_pu32(msg, &fcall->hdr.fid);
		ixp_pu64(msg, &fcall->tread.offset);
		ixp_pu32(msg, &fcall->tread.count);
		break;
	case RRead:
		ixp_pu32(msg, &fcall->rread.count);
		ixp_pdata(msg, &fcall->rread.data, fcall->rread.count);
		break;
	case TWrite:
		ixp_pu32(msg, &fcall->hdr.fid);
		ixp_pu64(msg, &fcall->twrite.offset);
		ixp_pu32(msg, &fcall->twrite.count);
		ixp_pdata(msg, &fcall->twrite.data, fcall->twrite.count);
		break;
	case RWrite:
		ixp_pu32(msg, &fcall->rwrite.count);
		break;
	case TClunk:
	case TRemove:
	case TStat:
		ixp_pu32(msg, &fcall->hdr.fid);
		break;
	case RStat:
		ixp_pu16(msg, &fcall->rstat.nstat);
		ixp_pdata(msg, (char**)&fcall->rstat.stat, fcall->rstat.nstat);
		break;
	case TWStat: {
		ushort size;
		ixp_pu32(msg, &fcall->hdr.fid);
		ixp_pu16(msg, &size);
		ixp_pstat(msg, &fcall->twstat.stat);
		break;
		}
	}
}

uint
ixp_fcall2msg(IxpMsg *msg, Fcall *fcall) {
	ulong size;

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
ixp_msg2fcall(IxpMsg *msg, Fcall *fcall) {
	msg->pos = msg->data + SDWord;
	msg->mode = MsgUnpack;
	ixp_pfcall(msg, fcall);

	if(msg->pos > msg->end)
		return 0;

	return msg->pos - msg->data;
}

