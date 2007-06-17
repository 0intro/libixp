/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ixp.h"

IxpConn *
ixp_listen(IxpServer *s, int fd, void *aux,
		void (*read)(IxpConn *c),
		void (*close)(IxpConn *c)
		) {
	IxpConn *c;

	c = ixp_emallocz(sizeof(IxpConn));
	c->fd = fd;
	c->aux = aux;
	c->srv = s;
	c->read = read;
	c->close = close;
	c->next = s->conn;
	s->conn = c;
	return c;
}

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

char *
ixp_serverloop(IxpServer *s) {
	int r;

	s->running = 1;
	while(s->running) {
		if(s->preselect)
			s->preselect(s);
		prepare_select(s);
		r = select(s->maxfd + 1, &s->rd, 0, 0, 0);
		if(r < 0) {
			if(errno == EINTR)
				continue;
			return "fatal select error";
		}
		handle_conns(s);
	}
	return nil;
}

void
ixp_server_close(IxpServer *s) {
	IxpConn *c, *next;
	for(c = s->conn; c; c = next) {
		next = c->next;
		ixp_hangup(c);
	}
}
