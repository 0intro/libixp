/* Copyright ©2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "ixp.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int
mread(int fd, Message *msg, uint count) {
	int r, n;

	n = msg->end - msg->pos;
	if(n <= 0) {
		errstr = "buffer full";
		return -1;
	}
	if(n > count)
		n = count;

	r = read(fd, msg->pos, n);
	if(r > 0)
		msg->pos += r;
	return r;
}

static int
readn(int fd, Message *msg, uint count) {
	uint num;
	int r;

	errstr = nil;
	num = count;
	while(num > 0) {
		r = mread(fd, msg, num);
		if(r < 1) {
			if(errstr == nil)
				errstr = "broken pipe";
			else if(errno == EINTR)
				continue;
			return count - num;
		}
		num -= r;
	}
	return count - num;
}

uint
ixp_sendmsg(int fd, Message *msg) {
	int r;

	msg->pos = msg->data;
	while(msg->pos < msg->end) {
		r = write(fd, msg->pos, msg->end - msg->pos);
		if(r < 1) {
			if(errno == EINTR)
				continue;
			errstr = "broken pipe";
			return 0;
		}
		msg->pos += r;
	}
	return msg->pos - msg->data;
}

uint
ixp_recvmsg(int fd, Message *msg) {
	enum { SSize = 4 };
	uint msize, size;

	msg->mode = MsgUnpack;
	msg->pos = msg->data;
	msg->end = msg->data + msg->size;
	if(readn(fd, msg, SSize) != SSize)
		return 0;

	msg->pos = msg->data;
	ixp_pu32(msg, &msize);

	size = msize - SSize;
	if(msg->pos + size >= msg->end) {
		errstr = "message too large";
		return 0;
	}
	if(readn(fd, msg, size) != size) {
		errstr = "message incomplete";
		return 0;
	}

	msg->end = msg->pos;
	return msize;
}
