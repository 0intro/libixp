/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "ixp.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* packode/unpackode stuff */

void
ixp_pack_u8(uchar **msg, int *msize, uchar val) {
	if(!msize || (*msize -= 1) >= 0)
		*(*msg)++ = val;
}

void
ixp_unpack_u8(uchar **msg, int *msize, uchar *val) {
	if(!msize || (*msize -= 1) >= 0)
		*val = *(*msg)++;
}

void
ixp_pack_u16(uchar **msg, int *msize, ushort val) {
	if(!msize || (*msize -= 2) >= 0) {
		*(*msg)++ = val;
		*(*msg)++ = val >> 8;
	}
}

void
ixp_unpack_u16(uchar **msg, int *msize, ushort *val) {
	if(!msize || (*msize -= 2) >= 0) {
		*val = *(*msg)++;
		*val |= *(*msg)++ << 8;
	}
}

void
ixp_pack_u32(uchar **msg, int *msize, uint val) {
	if(!msize || (*msize -= 4) >= 0) {
		*(*msg)++ = val;
		*(*msg)++ = val >> 8;
		*(*msg)++ = val >> 16;
		*(*msg)++ = val >> 24;
	}
}

void
ixp_unpack_u32(uchar **msg, int *msize, uint *val) {
	if(!msize || (*msize -= 4) >= 0) {
		*val = *(*msg)++;
		*val |= *(*msg)++ << 8;
		*val |= *(*msg)++ << 16;
		*val |= *(*msg)++ << 24;
	}
}

void
ixp_pack_u64(uchar **msg, int *msize, uvlong val) {
	if(!msize || (*msize -= 8) >= 0) {
		*(*msg)++ = val;
		*(*msg)++ = val >> 8;
		*(*msg)++ = val >> 16;
		*(*msg)++ = val >> 24;
		*(*msg)++ = val >> 32;
		*(*msg)++ = val >> 40;
		*(*msg)++ = val >> 48;
		*(*msg)++ = val >> 56;
	}
}

void
ixp_unpack_u64(uchar **msg, int *msize, uvlong *val) {
	if(!msize || (*msize -= 8) >= 0) {
		*val |= *(*msg)++;
		*val |= *(*msg)++ << 8;
		*val |= *(*msg)++ << 16;
		*val |= *(*msg)++ << 24;
		*val |= (uvlong)*(*msg)++ << 32;
		*val |= (uvlong)*(*msg)++ << 40;
		*val |= (uvlong)*(*msg)++ << 48;
		*val |= (uvlong)*(*msg)++ << 56;
	}
}

void
ixp_pack_string(uchar **msg, int *msize, const char *s) {
	ushort len = s ? strlen(s) : 0;
	ixp_pack_u16(msg, msize, len);
	if(s)
		ixp_pack_data(msg, msize, (void *)s, len);
}

void
ixp_unpack_strings(uchar **msg, int *msize, ushort n, char **strings) {
	uchar *s;
	uint i, size;
	ushort len;

	size = *msize;
	s = *msg;
	for(i=0; i<n && size > 0; i++) {
		ixp_unpack_u16(&s, &size, &len);
		s += len;
		size -= len;
	}
	if(size < 0)
		size = 0;
	size = *msize - size + n;
	if(size <= 0) {
		/* So we don't try to free some random value */
		*strings = NULL;
		return;
	}
	s = ixp_emalloc(size);
	for(i=0; i < n; i++) {
		ixp_unpack_u16(msg, msize, &len);
		if(!msize || (*msize -= len) < 0)
			return;
		memcpy(s, *msg, len);
		s[len] = '\0';
		strings[i] = (char *)s;
		*msg += len;
		s += len + 1;
	}
}

void
ixp_unpack_string(uchar **msg, int *msize, char **string, ushort *len) {
	ixp_unpack_u16(msg, msize, len);
	*string = NULL;
	if (!msize || (*msize -= *len) >= 0) {
		*string = ixp_emalloc(*len+1);
		if(*len)
			memcpy(*string, *msg, *len);
		(*string)[*len] = 0;
		*msg += *len;
	}
}

void
ixp_pack_data(uchar **msg, int *msize, uchar *data, uint datalen) {
	if(!msize || (*msize -= datalen) >= 0) {
		memcpy(*msg, data, datalen);
		*msg += datalen;
	}
}

void
ixp_unpack_data(uchar **msg, int *msize, uchar **data, uint datalen) {
	if(!msize || (*msize -= datalen) >= 0) {
		*data = ixp_emallocz(datalen);
		memcpy(*data, *msg, datalen);
		*msg += datalen;
	}
}

void
ixp_pack_prefix(uchar *msg, uint size, uchar id,
		ushort tag)
{
	ixp_pack_u32(&msg, 0, size);
	ixp_pack_u8(&msg, 0, id);
	ixp_pack_u16(&msg, 0, tag);
}

void
ixp_unpack_prefix(uchar **msg, uint *size, uchar *id,
		ushort *tag)
{
	int msize;
	ixp_unpack_u32(msg, NULL, size);
	msize = *size;
	ixp_unpack_u8(msg, &msize, id);
	ixp_unpack_u16(msg, &msize, tag);
}

void
ixp_pack_qid(uchar **msg, int *msize, Qid * qid) {
	ixp_pack_u8(msg, msize, qid->type);
	ixp_pack_u32(msg, msize, qid->version);
	ixp_pack_u64(msg, msize, qid->path);
}

void
ixp_unpack_qid(uchar **msg, int *msize, Qid * qid) {
	ixp_unpack_u8(msg, msize, &qid->type);
	ixp_unpack_u32(msg, msize, &qid->version);
	ixp_unpack_u64(msg, msize, &qid->path);
}

void
ixp_pack_stat(uchar **msg, int *msize, Stat * stat) {
	ixp_pack_u16(msg, msize, ixp_sizeof_stat(stat) - sizeof(ushort));
	ixp_pack_u16(msg, msize, stat->type);
	ixp_pack_u32(msg, msize, stat->dev);
	ixp_pack_qid(msg, msize, &stat->qid);
	ixp_pack_u32(msg, msize, stat->mode);
	ixp_pack_u32(msg, msize, stat->atime);
	ixp_pack_u32(msg, msize, stat->mtime);
	ixp_pack_u64(msg, msize, stat->length);
	ixp_pack_string(msg, msize, stat->name);
	ixp_pack_string(msg, msize, stat->uid);
	ixp_pack_string(msg, msize, stat->gid);
	ixp_pack_string(msg, msize, stat->muid);
}

void
ixp_unpack_stat(uchar **msg, int *msize, Stat * stat) {
	ushort dummy;

	*msg += sizeof(ushort);
	ixp_unpack_u16(msg, msize, &stat->type);
	ixp_unpack_u32(msg, msize, &stat->dev);
	ixp_unpack_qid(msg, msize, &stat->qid);
	ixp_unpack_u32(msg, msize, &stat->mode);
	ixp_unpack_u32(msg, msize, &stat->atime);
	ixp_unpack_u32(msg, msize, &stat->mtime);
	ixp_unpack_u64(msg, msize, &stat->length);
	ixp_unpack_string(msg, msize, &stat->name, &dummy);
	ixp_unpack_string(msg, msize, &stat->uid, &dummy);
	ixp_unpack_string(msg, msize, &stat->gid, &dummy);
	ixp_unpack_string(msg, msize, &stat->muid, &dummy);
}
