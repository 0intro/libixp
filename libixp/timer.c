/* Copyright ©2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>
#include "ixp_local.h"

/* This really needn't be threadsafe, as it has little use in
 * threaded programs, but it is, nonetheless.
 */

static long	lastid = 1;

long
ixp_msec(void) {
	timeval tv;

	if(gettimeofday(&tv, 0) < 0)
		return -1;
	return tv.tv_sec*1000 + tv.tv_usec/1000;
}

long
ixp_settimer(IxpServer *s, long msec, void (*fn)(long, void*), void *aux) {
	Timer **tp;
	Timer *t;
	long time;

	time = ixp_msec();
	if(time == -1)
		return -1;
	msec += time;

	t = emallocz(sizeof *t);
	thread->lock(&s->lk);
	t->id = lastid++;
	t->msec = msec;
	t->fn = fn;
	t->aux = aux;

	for(tp=&s->timer; *tp; tp=&tp[0]->link)
		if(tp[0]->msec < msec)
			break;
	t->link = *tp;
	*tp = t;
	thread->unlock(&s->lk);
	return t->id;
}

int
ixp_unsettimer(IxpServer *s, long id) {
	Timer **tp;
	Timer *t;

	thread->lock(&s->lk);
	for(tp=&s->timer; (t=*tp); tp=&t->link)
		if(t->id == id)
			break;
	if(t) {
		*tp = t->link;
		free(t);
	}
	thread->unlock(&s->lk);
	return t != nil;
}

long
ixp_nexttimer(IxpServer *s) {
	Timer *t;
	long time, ret;

	SET(time);
	thread->lock(&s->lk);
	while((t = s->timer)) {
		time = ixp_msec();
		if(t->msec > time)
			break;
		s->timer = t->link;

		thread->unlock(&s->lk);
		t->fn(t->id, t->aux);
		free(t);
		thread->lock(&s->lk);
	}
	ret = 0;
	if(t)
		ret = t->msec - time;
	thread->unlock(&s->lk);
	return ret;
}

