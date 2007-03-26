/* Copyright ©2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ixp.h"

enum {
	SByte = 1,
	SWord = 2,
	SDWord = 4,
	SQWord = 8,
};

void
ixp_puint(Message *msg, uint size, uint *val) {
	int v;

	if(msg->pos + size <= msg->end) {
		switch(msg->mode) {
		case MsgPack:
			v = *val;
			switch(size) {
			case SDWord:
				msg->pos[3] = v>>24;
				msg->pos[2] = v>>16;
			case SWord:
				msg->pos[1] = v>>8;
			case SByte:
				msg->pos[0] = v;
				break;
			}
		case MsgUnpack:
			v = 0;
			switch(size) {
			case SDWord:
				v |= msg->pos[3]<<24;
				v |= msg->pos[2]<<16;
			case SWord:
				v |= msg->pos[1]<<8;
			case SByte:
				v |= msg->pos[0];
				break;
			}
			*val = v;
		}
	}
	msg->pos += size;
}

void
ixp_pu32(Message *msg, uint *val) {
	ixp_puint(msg, SDWord, val);
}
void
ixp_pu8(Message *msg, uchar *val) {
	uint v;

	v = *val;
	ixp_puint(msg, SByte, &v);
	*val = (uchar)v;
}
void
ixp_pu16(Message *msg, ushort *val) {
	uint v;

	v = *val;
	ixp_puint(msg, SWord, &v);
	*val = (ushort)v;
}
void
ixp_pu64(Message *msg, uvlong *val) {
	uint vl, vb;

	vl = (uint)*val;
	vb = (uint)(*val>>32);
	ixp_puint(msg, SDWord, &vl);
	ixp_puint(msg, SDWord, &vb);
	*val = vl | ((uvlong)vb<<32);
}

void
ixp_pstring(Message *msg, char **s) {
	ushort len;

	if(msg->mode == MsgPack)
		len = strlen(*s);
	ixp_pu16(msg, &len);

	if(msg->pos + len <= msg->end) {
		if(msg->mode == MsgUnpack) {
			*s = ixp_emalloc(len + 1);
			memcpy(*s, msg->pos, len);
			(*s)[len] = '\0';
		}else
			memcpy(msg->pos, *s, len);
	}
	msg->pos += len;
}

void
ixp_pstrings(Message *msg, ushort *num, char *strings[]) {
	uchar *s;
	uint i, size;
	ushort len;

	ixp_pu16(msg, num);
	if(*num > IXP_MAX_WELEM) {
		msg->pos = msg->end+1;
		return;
	}

	if(msg->mode == MsgUnpack) {
		s = msg->pos;
		size = 0;
		for(i=0; i < *num; i++) {
			ixp_pu16(msg, &len);
			msg->pos += len;
			size += len;
			if(msg->pos > msg->end)
				return;
		}
		msg->pos = s;
		size += *num;
		s = ixp_emalloc(size);
	}

	for(i=0; i < *num; i++) {
		if(msg->mode == MsgPack)
			len = strlen(strings[i]);
		ixp_pu16(msg, &len);

		if(msg->mode == MsgUnpack) {
			memcpy(s, msg->pos, len);
			strings[i] = s;
			s += len;
			msg->pos += len;
			*s++ = '\0';
		}else
			ixp_pdata(msg, &strings[i], len);
	}
}

void
ixp_pdata(Message *msg, char **data, uint len) {
	if(msg->pos + len <= msg->end) {
		if(msg->mode == MsgUnpack) {
			*data = ixp_emalloc(len);
			memcpy(*data, msg->pos, len);
		}else
			memcpy(msg->pos, *data, len);
	}
	msg->pos += len;
}

void
ixp_pqid(Message *msg, Qid *qid) {
	ixp_pu8(msg, &qid->type);
	ixp_pu32(msg, &qid->version);
	ixp_pu64(msg, &qid->path);
}

void
ixp_pqids(Message *msg, ushort *num, Qid qid[]) {
	int i;

	ixp_pu16(msg, num);
	if(*num > IXP_MAX_WELEM) {
		msg->pos = msg->end+1;
		return;
	}

	for(i = 0; i < *num; i++)
		ixp_pqid(msg, &qid[i]);
}

void
ixp_pstat(Message *msg, Stat *stat) {
	ushort size;

	if(msg->mode == MsgPack)
		size = ixp_sizeof_stat(stat);

	ixp_pu16(msg, &size);
	ixp_pu16(msg, &stat->type);
	ixp_pu32(msg, &stat->dev);
	ixp_pqid(msg, &stat->qid);
	ixp_pu32(msg, &stat->mode);
	ixp_pu32(msg, &stat->atime);
	ixp_pu32(msg, &stat->mtime);
	ixp_pu64(msg, &stat->length);
	ixp_pstring(msg, &stat->name);
	ixp_pstring(msg, &stat->uid);
	ixp_pstring(msg, &stat->gid);
	ixp_pstring(msg, &stat->muid);
}
