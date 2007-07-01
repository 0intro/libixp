/* from fcall(3) in plan9port */
struct IxpFcall {
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
			IxpQid qid; /* +Rattach */
			uint iounit;
		};
		struct { /* Rauth */
			IxpQid aqid;
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
			IxpQid	wqid[IXP_MAX_WELEM];
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
};
