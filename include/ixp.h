/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */

#include <sys/types.h>

#undef uchar
#undef ushort
#undef uint
#undef ulong
#undef vlong
#undef uvlong
#define uchar	_ixpuchar
#define ushort	_ixpushort
#define uint	_ixpuint
#define ulong	_ixpulong
#define vlong	_ixpvlong
#define uvlong	_ixpuvlong
typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef long long		vlong;
typedef unsigned long long	uvlong;

char *errstr;

#undef nil
#define nil ((void*)0)

#define IXP_VERSION	"9P2000"
#define IXP_NOTAG	((ushort)~0)	/*Dummy tag */

enum {
	IXP_NOFID = ~0U,
	IXP_MAX_VERSION = 32,
	IXP_MAX_MSG = 65535,
	IXP_MAX_ERROR = 128,
	IXP_MAX_CACHE = 32,
	IXP_MAX_FLEN = 128,
	IXP_MAX_ULEN = 32,
	IXP_MAX_WELEM = 16,
};

/* 9P message types */
enum {	TVersion = 100,
	RVersion,
	TAuth = 102,
	RAuth,
	TAttach = 104,
	RAttach,
	TError = 106, /* illegal */
	RError,
	TFlush = 108,
	RFlush,
	TWalk = 110,
	RWalk,
	TOpen = 112,
	ROpen,
	TCreate = 114,
	RCreate,
	TRead = 116,
	RRead,
	TWrite = 118,
	RWrite,
	TClunk = 120,
	RClunk,
	TRemove = 122,
	RRemove,
	TStat = 124,
	RStat,
	TWStat = 126,
	RWStat,
};

/* from libc.h in p9p */
enum {	P9_OREAD	= 0,	/* open for read */
	P9_OWRITE	= 1,	/* write */
	P9_ORDWR	= 2,	/* read and write */
	P9_OEXEC	= 3,	/* execute, == read but check execute permission */
	P9_OTRUNC	= 16,	/* or'ed in (except for exec), truncate file first */
	P9_OCEXEC	= 32,	/* or'ed in, close on exec */
	P9_ORCLOSE	= 64,	/* or'ed in, remove on close */
	P9_ODIRECT	= 128,	/* or'ed in, direct access */
	P9_ONONBLOCK	= 256,	/* or'ed in, non-blocking call */
	P9_OEXCL	= 0x1000,	/* or'ed in, exclusive use (create only) */
	P9_OLOCK	= 0x2000,	/* or'ed in, lock after opening */
	P9_OAPPEND	= 0x4000	/* or'ed in, append only */
};

/* bits in Qid.type */
enum {	QTDIR		= 0x80,	/* type bit for directories */
	QTAPPEND	= 0x40,	/* type bit for append only files */
	QTEXCL		= 0x20,	/* type bit for exclusive use files */
	QTMOUNT		= 0x10,	/* type bit for mounted channel */
	QTAUTH		= 0x08,	/* type bit for authentication file */
	QTTMP		= 0x04,	/* type bit for non-backed-up file */
	QTSYMLINK	= 0x02,	/* type bit for symbolic link */
	QTFILE		= 0x00	/* type bits for plain file */
};

/* bits in Dir.mode */
enum {
	P9_DMWRITE	= 0x2,		/* mode bit for write permission */
	P9_DMREAD	= 0x4,		/* mode bit for read permission */
	P9_DMEXEC	= 0x1,		/* mode bit for execute permission */
};
#define P9_DMDIR	0x80000000	/* mode bit for directories */
#define P9_DMAPPEND	0x40000000	/* mode bit for append only files */
#define P9_DMEXCL	0x20000000	/* mode bit for exclusive use files */
#define P9_DMMOUNT	0x10000000	/* mode bit for mounted channel */
#define P9_DMAUTH	0x08000000	/* mode bit for authentication file */
#define P9_DMTMP	0x04000000	/* mode bit for non-backed-up file */
#define P9_DMSYMLINK	0x02000000	/* mode bit for symbolic link (Unix, 9P2000.u) */
#define P9_DMDEVICE	0x00800000	/* mode bit for device file (Unix, 9P2000.u) */
#define P9_DMNAMEDPIPE	0x00200000	/* mode bit for named pipe (Unix, 9P2000.u) */
#define P9_DMSOCKET	0x00100000	/* mode bit for socket (Unix, 9P2000.u) */
#define P9_DMSETUID	0x00080000	/* mode bit for setuid (Unix, 9P2000.u) */
#define P9_DMSETGID	0x00040000	/* mode bit for setgid (Unix, 9P2000.u) */

enum { MsgPack, MsgUnpack, };
typedef struct Message Message;
struct Message {
	uchar *data;
	uchar *pos;
	uchar *end;
	uint size;
	uint mode;
};

typedef struct Qid Qid;
struct Qid {
	uchar type;
	uint version;
	uvlong path;
	/* internal use only */
	uchar dir_type;
};

#include <ixp_fcall.h>

/* stat structure */
typedef struct Stat {
	ushort type;
	uint dev;
	Qid qid;
	uint mode;
	uint atime;
	uint mtime;
	uvlong length;
	char *name;
	char *uid;
	char *gid;
	char *muid;
} Stat;

typedef struct IxpServer IxpServer;
typedef struct IxpConn IxpConn;
typedef struct Intmap Intmap;

typedef struct Intlist Intlist;
struct Intmap {
	ulong nhash;
	Intlist	**hash;
};

struct IxpConn {
	IxpServer	*srv;
	void		*aux;
	int		fd;
	void		(*read) (IxpConn *);
	void		(*close) (IxpConn *);
	char		closed;

	/* Implementation details */
	/* do not use */
	IxpConn		*next;
};

struct IxpServer {
	int running;
	IxpConn *conn;
	int maxfd;
	fd_set rd;
};


typedef struct IxpClient IxpClient;
typedef struct IxpCFid IxpCFid;

struct IxpClient {
	int	fd;
	uint	msize;
	uint	lastfid;

	IxpCFid	*freefid;
	Message	msg;
};

struct IxpCFid {
	uint	fid;
	Qid	qid;
	uchar	mode;
	uint	open;
	uint	iounit;
	uvlong	offset;
	/* internal use only */
	IxpClient *client;
	IxpCFid *next;
};

typedef struct Ixp9Conn Ixp9Conn;
typedef struct Fid {
	Ixp9Conn		*conn;
	Intmap		*map;
	char		*uid;
	void		*aux;
	ulong	fid;
	Qid		qid;
	signed char	omode;
} Fid;

typedef struct Ixp9Req Ixp9Req;
struct Ixp9Req {
	Ixp9Conn	*conn;
	Fid	*fid;
	Fid	*newfid;
	Ixp9Req	*oldreq;
	Fcall	ifcall;
	Fcall	ofcall;
	void	*aux;
};

typedef struct Ixp9Srv {
	void (*attach)(Ixp9Req *r);
	void (*clunk)(Ixp9Req *r);
	void (*create)(Ixp9Req *r);
	void (*flush)(Ixp9Req *r);
	void (*open)(Ixp9Req *r);
	void (*read)(Ixp9Req *r);
	void (*remove)(Ixp9Req *r);
	void (*stat)(Ixp9Req *r);
	void (*walk)(Ixp9Req *r);
	void (*write)(Ixp9Req *r);
	void (*freefid)(Fid *f);
} Ixp9Srv;

/* client.c */
IxpClient *ixp_mount(char *address);
IxpClient *ixp_mountfd(int fd);
void ixp_unmount(IxpClient *c);
IxpCFid *ixp_create(IxpClient *c, char *name, uint perm, uchar mode);
IxpCFid *ixp_open(IxpClient *c, char *name, uchar mode);
int ixp_remove(IxpClient *c, char *path);
Stat *ixp_stat(IxpClient *c, char *path);
int ixp_read(IxpCFid *f, void *buf, uint count);
int ixp_write(IxpCFid *f, void *buf, uint count);
int ixp_close(IxpCFid *f);

/* convert.c */
void ixp_pu8(Message *msg, uchar *val);
void ixp_pu16(Message *msg, ushort *val);
void ixp_pu32(Message *msg, uint *val);
void ixp_pu64(Message *msg, uvlong *val);
void ixp_pdata(Message *msg, char **data, uint len);
void ixp_pstring(Message *msg, char **s);
void ixp_pstrings(Message *msg, ushort *num, char *strings[]);
void ixp_pqid(Message *msg, Qid *qid);
void ixp_pqids(Message *msg, ushort *num, Qid qid[]);
void ixp_pstat(Message *msg, Stat *stat);

/* request.c */
void respond(Ixp9Req *r, char *error);
void serve_9pcon(IxpConn *c);

/* intmap.c */
void initmap(Intmap *m, ulong nhash, void *hash);
void incref_map(Intmap *m);
void decref_map(Intmap *m);
void freemap(Intmap *map, void (*destroy)(void*));
void execmap(Intmap *map, void (*destroy)(void*));
void *lookupkey(Intmap *map, ulong id);
void *insertkey(Intmap *map, ulong id, void *v);
void *deletekey(Intmap *map, ulong id);
int caninsertkey(Intmap *map, ulong id, void *v);

/* message.c */
ushort ixp_sizeof_stat(Stat *stat);
Message ixp_message(uchar *data, uint length, uint mode);
void ixp_freestat(Stat *s);
void ixp_freefcall(Fcall *fcall);
uint ixp_msg2fcall(Message *msg, Fcall *fcall);
uint ixp_fcall2msg(Message *msg, Fcall *fcall);

/* server.c */
IxpConn *ixp_listen(IxpServer *s, int fd, void *aux,
		void (*read)(IxpConn *c),
		void (*close)(IxpConn *c));
void ixp_hangup(IxpConn *c);
char *ixp_serverloop(IxpServer *s);
void ixp_server_close(IxpServer *s);

/* socket.c */
int ixp_dial(char *address);
int ixp_announce(char *address);

/* transport.c */
uint ixp_sendmsg(int fd, Message *msg);
uint ixp_recvmsg(int fd, Message *msg);

/* util.c */
void *ixp_emalloc(uint size);
void *ixp_emallocz(uint size);
void *ixp_erealloc(void *ptr, uint size);
char *ixp_estrdup(const char *str);
void ixp_eprint(const char *fmt, ...);
uint ixp_tokenize(char **result, uint reslen, char *str, char delim);
uint ixp_strlcat(char *dst, const char *src, uint siz);
