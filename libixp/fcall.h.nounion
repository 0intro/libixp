/* from fcall(3) in plan9port */
typedef struct Fcall {
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
	Qid qid; /* +Rattach */
	uint iounit;

	/* Rauth */
	Qid aqid;

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
	Qid	wqid[IXP_MAX_WELEM];

	/* Twrite */
	uvlong	offset; /* +Tread */

	/* +Rread */
	uint	count; /* +Tread */
	char		*data;

	/* Twstat, Rstat */
	ushort	nstat;
	uchar	*stat;
} Fcall;
