/* from fcall(3) in plan9port */
struct IxpFcall {
	uchar type;
	ushort tag;
	uint fid;

	/* Tversion, Rversion */
	uint msize;
	char	*version;

	/* Tflush */
	ushort oldtag;

	/* Rerror */
	char *ename;

	/* Ropen, Rcreate */
	IxpQid qid; /* +Rattach */
	uint iounit;

	/* Rauth */
	IxpQid aqid;

	/* Tauth, Tattach */
	uint	afid;
	char		*uname;
	char		*aname;

	/* Tcreate */
	uint	perm;
	char		*name;
	uchar	mode; /* +Topen */

	/* Twalk */
	uint	newfid;
	ushort	nwname;
	char	*wname[IXP_MAX_WELEM];

	/* Rwalk */
	ushort	nwqid;
	IxpQid	wqid[IXP_MAX_WELEM];

	/* Twrite */
	uvlong	offset; /* +Tread */

	/* +Rread */
	uint	count; /* +Tread */
	char		*data;

	/* Twstat, Rstat */
	ushort	nstat;
	uchar	*stat;
};
