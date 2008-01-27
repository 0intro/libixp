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

/**
 * Function: ixp_msec
 *
 * Returns:
 *	Returns the time since the Epoch in milliseconds.
 * Be aware that this may overflow.
 */
long
ixp_msec(void) {
	timeval tv;

	if(gettimeofday(&tv, 0) < 0)
		return -1;
	return tv.tv_sec*1000 + tv.tv_usec/1000;
}

/**
 * Function: ixp_settimer
 *
 * Params:
 *	msec - The timeout in milliseconds.
 *	fn - The function to call after P<msec> milliseconds
 *	     have elapsed.
 *	aux - An arbitrary argument to pass to P<fn> when it
 *	      is called.
 * 
 * Initializes a callback-based timer to be triggerred after
 * P<msec> milliseconds. The timer is passed its id number
 * and the value of P<aux>.
 *
 * Returns:
 *	Returns the new timer's unique id number.
 */
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

/**
 * Function: ixp_unsettimer
 *
 * Params:
 *	id - The id number of the timer to void.
 *
 * Voids the timer identified by P<id>.
 *
 * Returns:
 *	Returns true if a timer was stopped, false
 * otherwise.
 */
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

/**
 * Function: ixp_nexttimer
 *
 * Triggers any timers whose timeouts have ellapsed. This is
 * primarilly intended to be called from libixp's select
 * loop.
 *
 * Returns:
 *	Returns the number of milliseconds until the next
 * timer's timeout.
 */
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

