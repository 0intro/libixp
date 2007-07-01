#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <ixp.h>
#include <ixp_pthread.h>

extern char *(*_syserrstr)(void);
char *path;

typedef struct arg arg;
typedef struct arg2 arg2;
struct arg {
	Rendez r;
	IxpCFid *f;
	Channel *ch;
	int j, k;
};

struct arg2 {
	Rendez r;
	IxpClient *c;
	Channel *ch;
	int j;
};

void
readfile(IxpCFid *f, int j, int k) {
	Biobuf *b;
	char *buf, *p, *end;
	int n;

	fprint(2, "readfile(%p, %d, %d) iounit: %d\n", f, j, k, f->iounit);

	b = Bfdopen(1, OWRITE);
	Bprint(b, ":: %d %d: ", j, k);

	buf = ixp_emalloc(f->iounit);
	while((n = ixp_read(f, buf, f->iounit)) > 0) {
		fprint(2, "+readfile(%p, %d, %d) n=%d\n", f, j, k, n);
		end = buf+n;
		p = buf;
		for(p = buf; p < end; p++) {
			Bputc(b, *p);
			if(*p == '\n') {
				Bflush(b);
				Bprint(b, ":: %d %d: ", j, k);
			}
		}
	}

	fprint(2, "-readfile(%p, %d, %d) iounit: %d\n", f, j, k, f->iounit);
	Bputc(b, '\n');
	Bterm(b);
}

static void
_read(void *p) {
	arg *a;
	int i, k;

	a = p;
	k = a->k;
	print("Start _read: %d\n", a->j, k);

	qlock(a->r.l);
	sendul(a->ch, 0);
	rsleep(&a->r);
	print("Wake _read: %d\n", a->j, k);
	qunlock(a->r.l);

	for(i = 0; i < 15; i++)
		readfile(a->f, a->j, k);
}

static void
_open(void *p) {
	arg2 *a2;
	arg a;

	a2 = p;
	a.j = a2->j;
	memset(&a.r, 0, sizeof(a.r));
	a.r.l = mallocz(sizeof(QLock), 1);
	a.ch = chancreate(sizeof(ulong), 0);

	print("Start _open: %d\n", a2->j);

	qlock(a2->r.l);
	sendul(a2->ch, 0);
	rsleep(&a2->r);
	print("Wake _open: %d\n", a2->j);
	qunlock(a2->r.l);

	a.f = ixp_open(a2->c, path, OREAD);
	if(a.f == nil)
		sysfatal("can't open %q: %r\n", path);

	for(a.k = 0; a.k < 5; a.k++) {
		proccreate(_read, &a, mainstacksize);
		recvul(a.ch);
	}

	qlock(a.r.l);
	rwakeupall(&a.r);
	qunlock(a.r.l);
	recvul(chancreate(sizeof(ulong),0));
}

const char *_malloc_options = "A";

void
threadmain(int argc, char *argv[]) {
	arg2 a;
	char *address;

	USED(argc);
	USED(argv);
	address = "unix!/tmp/ns.kris.:0/wmii";
	address = "tcp!localhost!6663";
	path = "/n/local/var/log/messages";

	quotefmtinstall();

	_syserrstr = ixp_errbuf;
	if(ixp_pthread_init())
		sysfatal("can't init pthread: %r\n");

	a.c = ixp_mount(address);
	if(a.c == nil)
		sysfatal("can't mount: %r\n");

	memset(&a.r, 0, sizeof(a.r));
	a.r.l = mallocz(sizeof(QLock), 1);
	a.ch = chancreate(sizeof(ulong), 0);
	for(a.j = 0; a.j < 5; a.j++) {
		proccreate(_open, &a, mainstacksize);
		recvul(a.ch);
	}

	qlock(a.r.l);
	fprint(2, "qlock()\n");
	rwakeupall(&a.r);
	fprint(2, "wokeup\n");
	qunlock(a.r.l);
	fprint(2, "unlocked\n");
	recvul(chancreate(sizeof(ulong),0));
}

