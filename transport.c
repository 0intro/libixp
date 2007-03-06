/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
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

uint
ixp_send_message(int fd, void *msg, uint msize, char **errstr) {
	uint num = 0;
	int r;

	/* send message */
	while(num < msize) {
		r = write(fd, (uchar*)msg + num, msize - num);
		if(r == -1 && errno == EINTR)
			continue;
		if(r < 1) {
			*errstr = "broken pipe";
			return 0;
		}
		num += r;
	}
	return num;
}

static uint
ixp_recv_data(int fd, void *msg, uint msize, char **errstr) {
	uint num = 0;
	int r = 0;

	/* receive data */
	while(num < msize) {
		r = read(fd, (uchar*)msg + num, msize - num);
		if(r == -1 && errno == EINTR)
			continue;
		if(r < 1) {
			*errstr = "broken pipe";
			return 0;
		}
		num += r;
	}
	return num;
}

uint
ixp_recv_message(int fd, void *msg, uint msglen, char **errstr) {
	uint msize;

	/* receive header */
	if(ixp_recv_data(fd, msg, sizeof(uint), errstr) !=
			sizeof(uint))
		return 0;
	ixp_unpack_u32((void *)&msg, NULL, &msize);
	if(msize > msglen) {
		*errstr = "invalid message header";
		return 0;
	}
	/* receive message */
	if(ixp_recv_data(fd, msg, msize - sizeof(uint), errstr)
		!= msize - sizeof(uint))
		return 0;
	return msize;
}
