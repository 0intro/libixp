/* from fcall(3) in plan9port */
typedef struct Fcall {
	uchar type;
	ushort tag;
	uint fid;
	union {
		struct { /* Tversion, Rversion */
			uint msize;
			char	*version;
		};
		struct { /* Tflush */
			ushort oldtag;
		};
		struct { /* Rerror */
			char *ename;
		};
		struct { /* Ropen, Rcreate */
			Qid qid; /* +Rattach */
			uint iounit;
		};
		struct { /* Rauth */
			Qid aqid;
		};
		struct { /* Tauth, Tattach */
			uint	afid;
			char		*uname;
			char		*aname;
		};
		struct { /* Tcreate */
			uint	perm;
			char		*name;
			uchar	mode; /* +Topen */
		};
		struct { /* Twalk */
			uint	newfid;
			ushort	nwname;
			char	*wname[IXP_MAX_WELEM];
		};
		struct { /* Rwalk */
			ushort	nwqid;
			Qid	wqid[IXP_MAX_WELEM];
		};
		struct { /* Twrite */
			uvlong	offset; /* +Tread */
			/* +Rread */
			uint	count; /* +Tread */
			char		*data;
		};
		struct { /* Twstat, Rstat */
			ushort	nstat;
			uchar	*stat;
		};
	};
} Fcall;
