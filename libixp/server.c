/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ixp_local.h"

/**
 * Function: ixp_listen
 *
 * Params:
 *	fs - The file descriptor on which to listen.
 *	aux - A piece of data to store in the connection's
 *	      T<IxpConn> data structure.
 *	read - The function to call when the connection has
 *	       data available to read.
 *	close - A cleanup function to call when the
 *	        connection is closed.
 *
 * Starts the server P<s> listening on P<fd>. The optional
 * callbacks are called as described, with the connections
 * T<IxpConn> data structure as their arguments.
 *
 * Returns:
 *	Returns the connection's new T<IxpConn> data
 * structure.
 *
 * S<IxpConn>
 */
IxpConn*
ixp_listen(IxpServer *s, int fd, void *aux,
		void (*read)(IxpConn *c),
		void (*close)(IxpConn *c)
		) {
	IxpConn *c;

	c = emallocz(sizeof(IxpConn));
	c->fd = fd;
	c->aux = aux;
	c->srv = s;
	c->read = read;
	c->close = close;
	c->next = s->conn;
	s->conn = c;
	return c;
}

/**
 * Function: ixp_hangup
 * Function: ixp_server_close
 *
 * ixp_hangup closes a connection, and stops the server
 * listening on it. It calls the connection's close
 * function, if it exists. ixp_server_close calls ixp_hangup
 * on all of the connections on which the server is
 * listening.
 */

void
ixp_hangup(IxpConn *c) {
	IxpServer *s;
	IxpConn **tc;

	s = c->srv;
	for(tc=&s->conn; *tc; tc=&(*tc)->next)
		if(*tc == c) break;
	assert(*tc == c);

	*tc = c->next;
	c->closed = 1;
	if(c->close)
		c->close(c);
	else
		shutdown(c->fd, SHUT_RDWR);

	close(c->fd);
	free(c);
}

void
ixp_server_close(IxpServer *s) {
	IxpConn *c, *next;

	for(c = s->conn; c; c = next) {
		next = c->next;
		ixp_hangup(c);
	}
}

static void
prepare_select(IxpServer *s) {
	IxpConn *c;

	FD_ZERO(&s->rd);
	for(c = s->conn; c; c = c->next)
		if(c->read) {
			if(s->maxfd < c->fd)
				s->maxfd = c->fd;
			FD_SET(c->fd, &s->rd);
		}
}

static void
handle_conns(IxpServer *s) {
	IxpConn *c, *n;
	for(c = s->conn; c; c = n) {
		n = c->next;
		if(FD_ISSET(c->fd, &s->rd))
			c->read(c);
	}
}

/**
 * Function: ixp_serverloop
 *
 * Enters the main loop of the server. Exits when
 * P<s>->running becomes false, or when select(2) returns an
 * error other than EINTR.
 *
 * S<IxpServer>
 *
 * Returns:
 *	Returns 0 when the loop exits normally, and 1 when
 * it exits on error. V<errno> or the return value of
 * ixp_errbuf(3) may be inspected.
 *
 */

int
ixp_serverloop(IxpServer *s) {
	timeval *tvp;
	timeval tv;
	long timeout;
	int r;

	s->running = 1;
	thread->initmutex(&s->lk);
	while(s->running) {
		if(s->preselect)
			s->preselect(s);

		tvp = nil;
		timeout = ixp_nexttimer(s);
		if(timeout > 0) {
			tv.tv_sec = timeout/1000;
			tv.tv_usec = timeout%1000 * 1000;
			tvp = &tv;
		}

		prepare_select(s);
		r = thread->select(s->maxfd + 1, &s->rd, 0, 0, tvp);
		if(r < 0) {
			if(errno == EINTR)
				continue;
			return 1;
		}
		handle_conns(s);
	}
	return 0;
}

