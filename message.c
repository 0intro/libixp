/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "ixp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IXP_QIDSZ (sizeof(unsigned char) + sizeof(unsigned int)\
		+ sizeof(unsigned long long))

static unsigned short
sizeof_string(const char *s) {
	return sizeof(unsigned short) + strlen(s);
}

unsigned short
ixp_sizeof_stat(Stat * stat) {
	return IXP_QIDSZ
		+ 2 * sizeof(unsigned short)
		+ 4 * sizeof(unsigned int)
		+ sizeof(unsigned long long)
		+ sizeof_string(stat->name)
		+ sizeof_string(stat->uid)
		+ sizeof_string(stat->gid)
		+ sizeof_string(stat->muid);
}

unsigned int
ixp_fcall2msg(void *msg, Fcall *fcall, unsigned int msglen) {
	unsigned int i = sizeof(unsigned char) +
		sizeof(unsigned short) + sizeof(unsigned int);
	int msize = msglen - i;
	unsigned char *p = msg + i;

	switch (fcall->type) {
	case TVERSION:
	case RVERSION:
		ixp_pack_u32(&p, &msize, fcall->data.rversion.msize);
		ixp_pack_string(&p, &msize, fcall->data.rversion.version);
		break;
	case TAUTH:
		ixp_pack_u32(&p, &msize, fcall->data.tauth.afid);
		ixp_pack_string(&p, &msize, fcall->data.tauth.uname);
		ixp_pack_string(&p, &msize, fcall->data.tauth.aname);
		break;
	case RAUTH:
		ixp_pack_qid(&p, &msize, &fcall->data.rauth.aqid);
		break;
	case RATTACH:
		ixp_pack_qid(&p, &msize, &fcall->data.rattach.qid);
		break;
	case TATTACH:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u32(&p, &msize, fcall->data.tattach.afid);
		ixp_pack_string(&p, &msize, fcall->data.tattach.uname);
		ixp_pack_string(&p, &msize, fcall->data.tattach.aname);
		break;
	case RERROR:
		ixp_pack_string(&p, &msize, fcall->data.rerror.ename);
		break;
	case TFLUSH:
		ixp_pack_u16(&p, &msize, fcall->data.tflush.oldtag);
		break;
	case TWALK:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u32(&p, &msize, fcall->data.twalk.newfid);
		ixp_pack_u16(&p, &msize, fcall->data.twalk.nwname);
		for(i = 0; i < fcall->data.twalk.nwname; i++)
			ixp_pack_string(&p, &msize, fcall->data.twalk.wname[i]);
		break;
	case RWALK:
		ixp_pack_u16(&p, &msize, fcall->data.rwalk.nwqid);
		for(i = 0; i < fcall->data.rwalk.nwqid; i++)
			ixp_pack_qid(&p, &msize, &fcall->data.rwalk.wqid[i]);
		break;
	case TOPEN:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u8(&p, &msize, fcall->data.topen.mode);
		break;
	case ROPEN:
	case RCREATE:
		ixp_pack_qid(&p, &msize, &fcall->data.rcreate.qid);
		ixp_pack_u32(&p, &msize, fcall->data.rcreate.iounit);
		break;
	case TCREATE:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_string(&p, &msize, fcall->data.tcreate.name);
		ixp_pack_u32(&p, &msize, fcall->data.tcreate.perm);
		ixp_pack_u8(&p, &msize, fcall->data.tcreate.mode);
		break;
	case TREAD:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u64(&p, &msize, fcall->data.tread.offset);
		ixp_pack_u32(&p, &msize, fcall->data.tread.count);
		break;
	case RREAD:
		ixp_pack_u32(&p, &msize, fcall->data.rread.count);
		ixp_pack_data(&p, &msize, (unsigned char *)fcall->data.rread.data, fcall->data.rread.count);
		break;
	case TWRITE:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u64(&p, &msize, fcall->data.twrite.offset);
		ixp_pack_u32(&p, &msize, fcall->data.twrite.count);
		ixp_pack_data(&p, &msize, (unsigned char *)fcall->data.twrite.data, fcall->data.twrite.count);
		break;
	case RWRITE:
		ixp_pack_u32(&p, &msize, fcall->data.rwrite.count);
		break;
	case TCLUNK:
	case TREMOVE:
	case TSTAT:
		ixp_pack_u32(&p, &msize, fcall->fid);
		break;
	case RSTAT:
		ixp_pack_u16(&p, &msize, fcall->data.rstat.nstat);
		ixp_pack_data(&p, &msize, fcall->data.rstat.stat, fcall->data.rstat.nstat);
		break;
	case TWSTAT:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u16(&p, &msize, fcall->data.twstat.nstat);
		ixp_pack_data(&p, &msize, fcall->data.twstat.stat, fcall->data.twstat.nstat);
		break;
	}
	if(msize < 0)
		return 0;
	msize = msglen - msize;
	ixp_pack_prefix(msg, msize, fcall->type, fcall->tag);
	return msize;
}

unsigned int
ixp_msg2fcall(Fcall *fcall, void *msg, unsigned int msglen) {
	int msize;
	unsigned int i, tsize;
	unsigned short len;
	unsigned char *p = msg;
	ixp_unpack_prefix(&p, (unsigned int *)&msize, &fcall->type, &fcall->tag);
	tsize = msize;

	if(msize > msglen)          /* bad message */
		return 0;
	switch (fcall->type) {
	case TVERSION:
	case RVERSION:
		ixp_unpack_u32(&p, &msize, &fcall->data.rversion.msize);
		ixp_unpack_string(&p, &msize, &fcall->data.rversion.version, &len);
		break;
	case TAUTH:
		ixp_unpack_u32(&p, &msize, &fcall->data.tauth.afid);
		ixp_unpack_string(&p, &msize, &fcall->data.tauth.uname, &len);
		ixp_unpack_string(&p, &msize, &fcall->data.tauth.aname, &len);
		break;
	case RAUTH:
		ixp_unpack_qid(&p, &msize, &fcall->data.rauth.aqid);
		break;
	case RATTACH:
		ixp_unpack_qid(&p, &msize, &fcall->data.rattach.qid);
		break;
	case TATTACH:
		ixp_unpack_u32(&p, &msize, &fcall->fid);
		ixp_unpack_u32(&p, &msize, &fcall->data.tattach.afid);
		ixp_unpack_string(&p, &msize, &fcall->data.tattach.uname, &len);
		ixp_unpack_string(&p, &msize, &fcall->data.tattach.aname, &len);
		break;
	case RERROR:
		ixp_unpack_string(&p, &msize, &fcall->data.rerror.ename, &len);
		break;
	case TFLUSH:
		ixp_unpack_u16(&p, &msize, &fcall->data.tflush.oldtag);
		break;
	case TWALK:
		ixp_unpack_u32(&p, &msize, &fcall->fid);
		ixp_unpack_u32(&p, &msize, &fcall->data.twalk.newfid);
		ixp_unpack_u16(&p, &msize, &fcall->data.twalk.nwname);
		ixp_unpack_strings(&p, &msize, fcall->data.twalk.nwname, fcall->data.twalk.wname);
		break;
	case RWALK:
		ixp_unpack_u16(&p, &msize, &fcall->data.rwalk.nwqid);
		for(i = 0; i < fcall->data.rwalk.nwqid; i++)
			ixp_unpack_qid(&p, &msize, &fcall->data.rwalk.wqid[i]);
		break;
	case TOPEN:
		ixp_unpack_u32(&p, &msize, &fcall->fid);
		ixp_unpack_u8(&p, &msize, &fcall->data.topen.mode);
		break;
	case ROPEN:
	case RCREATE:
		ixp_unpack_qid(&p, &msize, &fcall->data.rcreate.qid);
		ixp_unpack_u32(&p, &msize, &fcall->data.rcreate.iounit);
		break;
	case TCREATE:
		ixp_unpack_u32(&p, &msize, &fcall->fid);
		ixp_unpack_string(&p, &msize, &fcall->data.tcreate.name, &len);
		ixp_unpack_u32(&p, &msize, &fcall->data.tcreate.perm);
		ixp_unpack_u8(&p, &msize, &fcall->data.tcreate.mode);
		break;
	case TREAD:
		ixp_unpack_u32(&p, &msize, &fcall->fid);
		ixp_unpack_u64(&p, &msize, &fcall->data.tread.offset);
		ixp_unpack_u32(&p, &msize, &fcall->data.tread.count);
		break;
	case RREAD:
		ixp_unpack_u32(&p, &msize, &fcall->data.rread.count);
		ixp_unpack_data(&p, &msize, (void *)&fcall->data.rread.data, fcall->data.rread.count);
		break;
	case TWRITE:
		ixp_unpack_u32(&p, &msize, &fcall->fid);
		ixp_unpack_u64(&p, &msize, &fcall->data.twrite.offset);
		ixp_unpack_u32(&p, &msize, &fcall->data.twrite.count);
		ixp_unpack_data(&p, &msize, (void *)&fcall->data.twrite.data, fcall->data.twrite.count);
		break;
	case RWRITE:
		ixp_unpack_u32(&p, &msize, &fcall->data.rwrite.count);
		break;
	case TCLUNK:
	case TREMOVE:
	case TSTAT:
		ixp_unpack_u32(&p, &msize, &fcall->fid);
		break;
	case RSTAT:
		ixp_unpack_u16(&p, &msize, &len);
		ixp_unpack_data(&p, &msize, &fcall->data.rstat.stat, len);
		break;
	case TWSTAT:
		ixp_unpack_u32(&p, &msize, &fcall->fid);
		ixp_unpack_u16(&p, &msize, &len);
		ixp_unpack_data(&p, &msize, &fcall->data.twstat.stat, len);
		break;
	}
	if(msize > 0)
		return tsize;
	return 0;
}
