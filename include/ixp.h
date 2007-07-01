/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */

#include <stdarg.h>
#include <sys/types.h>
#include <sys/select.h>

#undef	uchar
#define	uchar	_ixpuchar
#undef	ushort
#define	ushort	_ixpushort
#undef	uint
#define	uint	_ixpuint
#undef	ulong
#define	ulong	_ixpulong
#undef	vlong
#define	vlong	_ixpvlong
#undef	uvlong
#define	uvlong	_ixpuvlong
typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef long long		vlong;
typedef unsigned long long	uvlong;

#define IXP_VERSION	"9P2000"
#define IXP_NOTAG	((ushort)~0)	/* Dummy tag */
#define IXP_NOFID	(~0U)

enum {
	IXP_MAX_VERSION = 32,
	IXP_MAX_MSG = 8192,
	IXP_MAX_ERROR = 128,
	IXP_MAX_CACHE = 32,
	IXP_MAX_FLEN = 128,
	IXP_MAX_ULEN = 32,
	IXP_MAX_WELEM = 16,
};

/* 9P message types */
enum {	P9_TVersion = 100,
	P9_RVersion,
	P9_TAuth = 102,
	P9_RAuth,
	P9_TAttach = 104,
	P9_RAttach,
	P9_TError = 106, /* illegal */
	P9_RError,
	P9_TFlush = 108,
	P9_RFlush,
	P9_TWalk = 110,
	P9_RWalk,
	P9_TOpen = 112,
	P9_ROpen,
	P9_TCreate = 114,
	P9_RCreate,
	P9_TRead = 116,
	P9_RRead,
	P9_TWrite = 118,
	P9_RWrite,
	P9_TClunk = 120,
	P9_RClunk,
	P9_TRemove = 122,
	P9_RRemove,
	P9_TStat = 124,
	P9_RStat,
	P9_TWStat = 126,
	P9_RWStat,
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
enum {	P9_QTDIR	= 0x80,	/* type bit for directories */
	P9_QTAPPEND	= 0x40,	/* type bit for append only files */
	P9_QTEXCL	= 0x20,	/* type bit for exclusive use files */
	P9_QTMOUNT	= 0x10,	/* type bit for mounted channel */
	P9_QTAUTH	= 0x08,	/* type bit for authentication file */
	P9_QTTMP	= 0x04,	/* type bit for non-backed-up file */
	P9_QTSYMLINK	= 0x02,	/* type bit for symbolic link */
	P9_QTFILE	= 0x00	/* type bits for plain file */
};

/* bits in Dir.mode */
enum {
	P9_DMEXEC	= 0x1,		/* mode bit for execute permission */
	P9_DMWRITE	= 0x2,		/* mode bit for write permission */
	P9_DMREAD	= 0x4,		/* mode bit for read permission */
};

/* Larger than int, can't be enum */
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

#ifdef IXP_NO_P9_
#  define TVersion P9_TVersion
#  define RVersion P9_RVersion
#  define TAuth P9_TAuth
#  define RAuth P9_RAuth
#  define TAttach P9_TAttach
#  define RAttach P9_RAttach
#  define TError P9_TError
#  define RError P9_RError
#  define TFlush P9_TFlush
#  define RFlush P9_RFlush
#  define TWalk P9_TWalk
#  define RWalk P9_RWalk
#  define TOpen P9_TOpen
#  define ROpen P9_ROpen
#  define TCreate P9_TCreate
#  define RCreate P9_RCreate
#  define TRead P9_TRead
#  define RRead P9_RRead
#  define TWrite P9_TWrite
#  define RWrite P9_RWrite
#  define TClunk P9_TClunk
#  define RClunk P9_RClunk
#  define TRemove P9_TRemove
#  define RRemove P9_RRemove
#  define TStat P9_TStat
#  define RStat P9_RStat
#  define TWStat P9_TWStat
#  define RWStat P9_RWStat
#  define OREAD P9_OREAD
#  define OWRITE P9_OWRITE
#  define ORDWR P9_ORDWR
#  define OEXEC P9_OEXEC
#  define OTRUNC P9_OTRUNC
#  define OCEXEC P9_OCEXEC
#  define ORCLOSE P9_ORCLOSE
#  define ODIRECT P9_ODIRECT
#  define ONONBLOCK P9_ONONBLOCK
#  define OEXCL P9_OEXCL
#  define OLOCK P9_OLOCK
#  define OAPPEND P9_OAPPEND
#  define QTDIR P9_QTDIR
#  define QTAPPEND P9_QTAPPEND
#  define QTEXCL P9_QTEXCL
#  define QTMOUNT P9_QTMOUNT
#  define QTAUTH P9_QTAUTH
#  define QTTMP P9_QTTMP
#  define QTSYMLINK P9_QTSYMLINK
#  define QTFILE P9_QTFILE
#  define DMDIR P9_DMDIR
#  define DMAPPEND P9_DMAPPEND
#  define DMEXCL P9_DMEXCL
#  define DMMOUNT P9_DMMOUNT
#  define DMAUTH P9_DMAUTH
#  define DMTMP P9_DMTMP
#  define DMSYMLINK P9_DMSYMLINK
#  define DMDEVICE P9_DMDEVICE
#  define DMNAMEDPIPE P9_DMNAMEDPIPE
#  define DMSOCKET P9_DMSOCKET
#  define DMSETUID P9_DMSETUID
#  define DMSETGID P9_DMSETGID
#endif

#ifdef IXP_P9_STRUCTS
#  define IxpFcall Fcall
#  define IxpFid Fid
#  define IxpQid Qid
#  define IxpStat Stat
#endif

typedef struct Intmap Intmap;
typedef struct Ixp9Conn Ixp9Conn;
typedef struct Ixp9Req Ixp9Req;
typedef struct Ixp9Srv Ixp9Srv;
typedef struct IxpCFid IxpCFid;
typedef struct IxpClient IxpClient;
typedef struct IxpConn IxpConn;
typedef struct IxpFcall IxpFcall;
typedef struct IxpFid IxpFid;
typedef struct IxpMsg IxpMsg;
typedef struct IxpQid IxpQid;
typedef struct IxpRpc IxpRpc;
typedef struct IxpServer IxpServer;
typedef struct IxpStat IxpStat;

typedef struct IxpMutex IxpMutex;
typedef struct IxpRWLock IxpRWLock;
typedef struct IxpRendez IxpRendez;
typedef struct IxpThread IxpThread;

/* Threading */
enum {
	IXP_ERRMAX = IXP_MAX_ERROR,
};

struct IxpMutex {
	void *aux;
};

struct IxpRWLock {
	void *aux;
};

struct IxpRendez {
	IxpMutex *mutex;
	void *aux;
};

enum { MsgPack, MsgUnpack, };
struct IxpMsg {
	uchar *data;
	uchar *pos;
	uchar *end;
	uint size;
	uint mode;
};

struct IxpQid {
	uchar type;
	uint version;
	uvlong path;
	/* internal use only */
	uchar dir_type;
};

#include <ixp_fcall.h>

/* stat structure */
struct IxpStat {
	ushort type;
	uint dev;
	IxpQid qid;
	uint mode;
	uint atime;
	uint mtime;
	uvlong length;
	char *name;
	char *uid;
	char *gid;
	char *muid;
};

struct IxpConn {
	IxpServer	*srv;
	void		*aux;
	int		fd;
	void		(*read)(IxpConn *);
	void		(*close)(IxpConn *);
	char		closed;

	/* Implementation details, do not use */
	IxpConn		*next;
};

struct IxpServer {
	IxpConn *conn;
	void (*preselect)(IxpServer*);
	void *aux;
	int running;
	int maxfd;
	fd_set rd;
};

struct IxpRpc {
	IxpClient *mux;
	IxpRpc *next;
	IxpRpc *prev;
	IxpRendez r;
	uint tag;
	IxpFcall *p;
	int waiting;
	int async;
};

struct IxpClient {
	int	fd;
	uint	msize;
	uint	lastfid;

	/* Implementation details */
	uint nwait;
	uint mwait;
	uint freetag;
	IxpCFid	*freefid;
	IxpMsg	rmsg;
	IxpMsg	wmsg;
	IxpMutex lk;
	IxpMutex rlock;
	IxpMutex wlock;
	IxpRendez tagrend;
	IxpRpc **wait;
	IxpRpc *muxer;
	IxpRpc sleep;
	int mintag;
	int maxtag;
};

struct IxpCFid {
	uint	fid;
	IxpQid	qid;
	uchar	mode;
	uint	open;
	uint	iounit;
	uvlong	offset;
	IxpClient *client;
	/* internal use only */
	IxpCFid *next;
	IxpMutex iolock;
};

struct IxpFid {
	char		*uid;
	void		*aux;
	ulong		fid;
	IxpQid		qid;
	signed char	omode;

	/* Implementation details */
	Ixp9Conn	*conn;
	Intmap		*map;
};

struct Ixp9Req {
	Ixp9Srv	*srv;
	IxpFid	*fid;
	IxpFid	*newfid;
	Ixp9Req	*oldreq;
	IxpFcall	ifcall;
	IxpFcall	ofcall;
	void	*aux;

	/* Implementation details */
	Ixp9Conn *conn;
};

struct Ixp9Srv {
	void *aux;
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
	void (*freefid)(IxpFid *f);
};

struct IxpThread {
	/* RWLock */
	int (*initrwlock)(IxpRWLock*);
	void (*rlock)(IxpRWLock*);
	int (*canrlock)(IxpRWLock*);
	void (*runlock)(IxpRWLock*);
	void (*wlock)(IxpRWLock*);
	int (*canwlock)(IxpRWLock*);
	void (*wunlock)(IxpRWLock*);
	void (*rwdestroy)(IxpRWLock*);
	/* Mutex */
	int (*initmutex)(IxpMutex*);
	void (*lock)(IxpMutex*);
	int (*canlock)(IxpMutex*);
	void (*unlock)(IxpMutex*);
	void (*mdestroy)(IxpMutex*);
	/* Rendez */
	int (*initrendez)(IxpRendez*);
	void (*sleep)(IxpRendez*);
	int (*wake)(IxpRendez*);
	int (*wakeall)(IxpRendez*);
	void (*rdestroy)(IxpRendez*);
	/* Other */
	char *(*errbuf)(void);
	ssize_t (*read)(int, void*, size_t);
	ssize_t (*write)(int, const void*, size_t);
	int (*select)(int, fd_set*, fd_set*, fd_set*, struct timeval*);
};

extern IxpThread *ixp_thread;
extern int (*ixp_vsnprint)(char*, int, char*, va_list);
extern char* (*ixp_vsmprint)(char*, va_list);

/* thread_*.c */
int ixp_taskinit(void);
int ixp_rubyinit(void);
int ixp_pthread_init(void);

#ifdef VARARGCK
# pragma varargck	argpos	ixp_print	2
# pragma varargck	argpos	ixp_werrstr	1
# pragma varargck	argpos	ixp_eprint	1
#endif

/* client.c */
IxpClient *ixp_mount(char *address);
IxpClient *ixp_mountfd(int fd);
void ixp_unmount(IxpClient *c);
IxpCFid *ixp_create(IxpClient *c, char *name, uint perm, uchar mode);
IxpCFid *ixp_open(IxpClient *c, char *name, uchar mode);
int ixp_remove(IxpClient *c, char *path);
IxpStat *ixp_stat(IxpClient *c, char *path);
long ixp_read(IxpCFid *f, void *buf, long count);
long ixp_write(IxpCFid *f, void *buf, long count);
long ixp_pread(IxpCFid *f, void *buf, long count, vlong offset);
long ixp_pwrite(IxpCFid *f, void *buf, long count, vlong offset);
int ixp_close(IxpCFid *f);
int ixp_print(IxpCFid *f, char *fmt, ...);
int ixp_vprint(IxpCFid *f, char *fmt, va_list ap);

/* convert.c */
void ixp_pu8(IxpMsg *msg, uchar *val);
void ixp_pu16(IxpMsg *msg, ushort *val);
void ixp_pu32(IxpMsg *msg, uint *val);
void ixp_pu64(IxpMsg *msg, uvlong *val);
void ixp_pdata(IxpMsg *msg, char **data, uint len);
void ixp_pstring(IxpMsg *msg, char **s);
void ixp_pstrings(IxpMsg *msg, ushort *num, char *strings[]);
void ixp_pqid(IxpMsg *msg, IxpQid *qid);
void ixp_pqids(IxpMsg *msg, ushort *num, IxpQid qid[]);
void ixp_pstat(IxpMsg *msg, IxpStat *stat);

/* error.h */
char *ixp_errbuf(void);
void ixp_errstr(char*, int);
void ixp_rerrstr(char*, int);
void ixp_werrstr(char*, ...);

/* request.c */
void respond(Ixp9Req *r, char *error);
void serve_9pcon(IxpConn *c);

/* message.c */
ushort ixp_sizeof_stat(IxpStat *stat);
IxpMsg ixp_message(uchar *data, uint length, uint mode);
void ixp_freestat(IxpStat *s);
void ixp_freefcall(IxpFcall *fcall);
uint ixp_msg2fcall(IxpMsg *msg, IxpFcall *fcall);
uint ixp_fcall2msg(IxpMsg *msg, IxpFcall *fcall);

/* server.c */
IxpConn *ixp_listen(IxpServer *s, int fd, void *aux,
		void (*read)(IxpConn *c),
		void (*close)(IxpConn *c));
void ixp_hangup(IxpConn *c);
int ixp_serverloop(IxpServer *s);
void ixp_server_close(IxpServer *s);

/* socket.c */
int ixp_dial(char *address);
int ixp_announce(char *address);

/* transport.c */
uint ixp_sendmsg(int fd, IxpMsg *msg);
uint ixp_recvmsg(int fd, IxpMsg *msg);

/* util.c */
void *ixp_emalloc(uint size);
void *ixp_emallocz(uint size);
void *ixp_erealloc(void *ptr, uint size);
char *ixp_estrdup(const char *str);
void ixp_eprint(const char *fmt, ...);
uint ixp_tokenize(char **result, uint reslen, char *str, char delim);
uint ixp_strlcat(char *dst, const char *src, uint siz);

