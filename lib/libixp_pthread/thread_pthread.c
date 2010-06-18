/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#define _XOPEN_SOURCE 600
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "ixp_local.h"

static IxpThread ixp_pthread;
static pthread_key_t errstr_k;

/**
 * Function: ixp_pthread_init
 *
 * This function initializes libixp for use in multithreaded
 * programs using the POSIX thread system. When using libixp in such
 * programs, this function must be called before any other libixp
 * functions. This function is part of libixp_pthread, which you
 * must explicitly link against.
 */
int
ixp_pthread_init() {
	int ret;

	IXP_ASSERT_VERSION;

	ret = pthread_key_create(&errstr_k, free);
	if(ret) {
		werrstr("can't create TLS value: %s", ixp_errbuf());
		return 1;
	}

	ixp_thread = &ixp_pthread;
	return 0;
}

static char*
errbuf(void) {
	char *ret;

	ret = pthread_getspecific(errstr_k);
	if(ret == nil) {
		ret = emallocz(IXP_ERRMAX);
		pthread_setspecific(errstr_k, (void*)ret);
	}
	return ret;
}

static void
mlock(IxpMutex *m) {
	pthread_mutex_lock(m->aux);
}

static int
mcanlock(IxpMutex *m) {
	return !pthread_mutex_trylock(m->aux);
}

static void
munlock(IxpMutex *m) {
	pthread_mutex_unlock(m->aux);
}

static void
mdestroy(IxpMutex *m) {
	pthread_mutex_destroy(m->aux);
	free(m->aux);
}

static int
initmutex(IxpMutex *m) {
	pthread_mutex_t *mutex;

	mutex = emalloc(sizeof *mutex);
	if(pthread_mutex_init(mutex, nil)) {
		free(mutex);
		return 1;
	}

	m->aux = mutex;
	return 0;
}

static void
rlock(IxpRWLock *rw) {
	pthread_rwlock_rdlock(rw->aux);
}

static int
canrlock(IxpRWLock *rw) {
	return !pthread_rwlock_tryrdlock(rw->aux);
}

static void
wlock(IxpRWLock *rw) {
	pthread_rwlock_rdlock(rw->aux);
}

static int
canwlock(IxpRWLock *rw) {
	return !pthread_rwlock_tryrdlock(rw->aux);
}

static void
rwunlock(IxpRWLock *rw) {
	pthread_rwlock_unlock(rw->aux);
}

static void
rwdestroy(IxpRWLock *rw) {
	pthread_rwlock_destroy(rw->aux);
	free(rw->aux);
}

static int
initrwlock(IxpRWLock *rw) {
	pthread_rwlock_t *rwlock;

	rwlock = emalloc(sizeof *rwlock);
	if(pthread_rwlock_init(rwlock, nil)) {
		free(rwlock);
		return 1;
	}

	rw->aux = rwlock;
	return 0;
}

static void
rsleep(IxpRendez *r) {
	pthread_cond_wait(r->aux, r->mutex->aux);
}

static int
rwake(IxpRendez *r) {
	pthread_cond_signal(r->aux);
	return 0;
}

static int
rwakeall(IxpRendez *r) {
	pthread_cond_broadcast(r->aux);
	return 0;
}

static void
rdestroy(IxpRendez *r) {
	pthread_cond_destroy(r->aux);
	free(r->aux);
}

static int
initrendez(IxpRendez *r) {
	pthread_cond_t *cond;

	cond = emalloc(sizeof *cond);
	if(pthread_cond_init(cond, nil)) {
		free(cond);
		return 1;
	}

	r->aux = cond;
	return 0;
}

static IxpThread ixp_pthread = {
	/* Mutex */
	.initmutex = initmutex,
	.lock = mlock,
	.canlock = mcanlock,
	.unlock = munlock,
	.mdestroy = mdestroy,
	/* RWLock */
	.initrwlock = initrwlock,
	.rlock = rlock,
	.canrlock = canrlock,
	.wlock = wlock,
	.canwlock = canwlock,
	.runlock = rwunlock,
	.wunlock = rwunlock,
	.rwdestroy = rwdestroy,
	/* Rendez */
	.initrendez = initrendez,
	.sleep = rsleep,
	.wake = rwake,
	.wakeall = rwakeall,
	.rdestroy = rdestroy,
	/* Other */
	.errbuf = errbuf,
	.read = read,
	.write = write,
	.select = select,
};

