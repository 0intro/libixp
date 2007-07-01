#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <task.h>
#include "ixp_local.h"
#include "ixp_task.h"

static IxpThread ixp_task;

int
ixp_taskinit() {
	ixp_thread = &ixp_task;
	return 0;
}

static char*
errbuf(void) {
	void **p;

	p = taskdata();
	if(*p == nil)
		*p = emallocz(IXP_ERRMAX);
	return *p;
}

/* Mutex */
static int
initmutex(IxpMutex *m) {
	m->aux = emallocz(sizeof(QLock));
	return 0;
}

static void
mdestroy(IxpMutex *m) {
	free(m->aux);
	m->aux = nil;
}

static void
mlock(IxpMutex *m) {
	qlock(m->aux);
}

static int
mcanlock(IxpMutex *m) {
	return canqlock(m->aux);
}

static void
munlock(IxpMutex *m) {
	qunlock(m->aux);
}

/* RWLock */
static int
initrwlock(IxpRWLock *rw) {
	rw->aux = emallocz(sizeof(RWLock));
	return 0;
}

static void
rwdestroy(IxpRWLock *rw) {
	free(rw->aux);
	rw->aux = nil;
}

static void
_rlock(IxpRWLock *rw) {
	rlock(rw->aux);
}

static int
_canrlock(IxpRWLock *rw) {
	return canrlock(rw->aux);
}

static void
_wlock(IxpRWLock *rw) {
	wlock(rw->aux);
}

static int
_canwlock(IxpRWLock *rw) {
	return canwlock(rw->aux);
}

static void
_runlock(IxpRWLock *rw) {
	runlock(rw->aux);
}

static void
_wunlock(IxpRWLock *rw) {
	wunlock(rw->aux);
}

/* Rendez */
static int
initrendez(IxpRendez *r) {
	r->aux = emallocz(sizeof(Rendez));
	return 0;
}

static void
rdestroy(IxpRendez *r) {
	free(r->aux);
	r->aux = nil;
}

static void
rsleep(IxpRendez *r) {
	Rendez *rz;

	rz = r->aux;
	rz->l = r->mutex->aux;
	tasksleep(rz);
}

static int
rwake(IxpRendez *r) {
	Rendez *rz;

	rz = r->aux;
	rz->l = r->mutex->aux;
	return taskwakeup(rz);
}

static int
rwakeall(IxpRendez *r) {
	Rendez *rz;

	rz = r->aux;
	rz->l = r->mutex->aux;
	return taskwakeupall(rz);
}

/* Yielding IO */
static ssize_t
_read(int fd, void *buf, size_t size) {
	fdnoblock(fd);
	return fdread(fd, buf, size);
}

static ssize_t
_write(int fd, const void *buf, size_t size) {
	fdnoblock(fd);
	return fdwrite(fd, (void*)buf, size);
}

static IxpThread ixp_task = {
	/* Mutex */
	.initmutex = initmutex,
	.lock = mlock,
	.canlock = mcanlock,
	.unlock = munlock,
	.mdestroy = mdestroy,
	/* RWLock */
	.initrwlock = initrwlock,
	.rlock = _rlock,
	.canrlock = _canrlock,
	.wlock = _wlock,
	.canwlock = _canwlock,
	.runlock = _runlock,
	.wunlock = _wunlock,
	.rwdestroy = rwdestroy,
	/* Rendez */
	.initrendez = initrendez,
	.sleep = rsleep,
	.wake = rwake,
	.wakeall = rwakeall,
	.rdestroy = rdestroy,
	/* Other */
	.errbuf = errbuf,
	.read = _read,
	.write = _write,
};

