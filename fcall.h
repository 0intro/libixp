/* from fcall(3) in plan9port */
typedef struct Fcall {
	unsigned char type;
	unsigned short tag;
	unsigned int fid;
	union {
		struct { /* Tversion, Rversion */
			unsigned int msize;
			char	*version;
		};
		struct { /* Tflush */
			unsigned short oldtag;
		};
		struct { /* Rerror */
			char *ename;
		};
		struct { /* Ropen, Rcreate */
			Qid qid; /* +Rattach */
			unsigned int iounit;
		};
		struct { /* Rauth */
			Qid aqid;
		};
		struct { /* Tauth, Tattach */
			unsigned int	afid;
			char		*uname;
			char		*aname;
		};
		struct { /* Tcreate */
			unsigned int	perm;
			char		*name;
			unsigned char	mode; /* +Topen */
		};
		struct { /* Twalk */
			unsigned int	newfid;
			unsigned short	nwname;
			char	*wname[IXP_MAX_WELEM];
		};
		struct { /* Rwalk */
			unsigned short	nwqid;
			Qid	wqid[IXP_MAX_WELEM];
		};
		struct { /* Twrite */
			unsigned long long	offset; /* +Tread */
			/* +Rread */
			unsigned int	count; /* +Tread */
			char		*data;
		};
		struct { /* Twstat, Rstat */
			unsigned short	nstat;
			unsigned char	*stat;
		};
	};
} Fcall;
