/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "ixp.h"

typedef struct sockaddr sockaddr;

static int
dial_unix(char *address) {
	struct sockaddr_un sa;
	socklen_t su_len;
	int fd;

	memset(&sa, 0, sizeof(sa));

	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, address, sizeof(sa.sun_path));
	su_len = sizeof(sa) + strlen(sa.sun_path);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd < 0) {
		errstr = strerror(errno);
		return -1;
	}
	if(connect(fd, (sockaddr*) &sa, su_len)) {
		errstr = strerror(errno);
		close(fd);
		return -1;
	}
	return fd;
}

static int
dial_tcp(char *host) {
	struct sockaddr_in sa;
	struct hostent *hp;
	char *port;
	uint prt;
	int fd;

	memset(&sa, 0, sizeof(sa));

	port = strrchr(host, '!');
	if(port == nil) {
		errstr = "no port provided";
		return -1;
	}
	*port++ = '\0';

	if(sscanf(port, "%u", &prt) != 1)
		return -1;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0) {
		errstr = strerror(errno);
		return -1;
	}

	hp = gethostbyname(host);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(prt);
	memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);

	if(connect(fd, (sockaddr*)&sa, sizeof(struct sockaddr_in))) {
		errstr = strerror(errno);
		close(fd);
		return -1;
	}
	return fd;
}

static int
announce_tcp(char *host) {
	struct sockaddr_in sa;
	struct hostent *he;
	char *port;
	uint prt;
	int fd;

	memset(&sa, 0, sizeof(sa));

	port = strrchr(host, '!');
	if(port == nil) {
		errstr = "no port provided";
		return -1;
	}

	*port++ = '\0';
	if(sscanf(port, "%u", &prt) != 1) {
		errstr = "invalid port number";
		return -1;
	}

	signal(SIGPIPE, SIG_IGN);
	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		errstr = "cannot open socket";
		return -1;
	}

	sa.sin_family = AF_INET;
	sa.sin_port = htons(prt);

	if(!strcmp(host, "*"))
		sa.sin_addr.s_addr = htonl(INADDR_ANY);
	else if((he = gethostbyname(host)))
		memcpy(&sa.sin_addr, he->h_addr, he->h_length);
	else {
		errstr = "cannot translate hostname to an address";
		return -1;
	}

	if(bind(fd, (sockaddr*)&sa, sizeof(struct sockaddr_in)) < 0) {
		errstr = strerror(errno);
		close(fd);
		return -1;
	}

	if(listen(fd, IXP_MAX_CACHE) < 0) {
		errstr = strerror(errno);
		close(fd);
		return -1;
	}
	return fd;
}

static int
announce_unix(char *file) {
	const int yes = 1;
	struct sockaddr_un sa;
	socklen_t su_len;
	int fd;

	memset(&sa, 0, sizeof(sa));

	signal(SIGPIPE, SIG_IGN);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd < 0) {
		errstr = "cannot open socket";
		return -1;
	}

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(yes)) < 0) {
		errstr = strerror(errno);
		close(fd);
		return -1;
	}

	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, file, sizeof(sa.sun_path));
	su_len = sizeof(sa) + strlen(sa.sun_path);

	unlink(file);

	if(bind(fd, (sockaddr*)&sa, su_len) < 0) {
		errstr = strerror(errno);
		close(fd);
		return -1;
	}

	chmod(file, S_IRWXU);
	if(listen(fd, IXP_MAX_CACHE) < 0) {
		errstr = strerror(errno);
		close(fd);
		return -1;
	}
	return fd;
}

int
ixp_dial(char *address) {
	char *addr, *type;
	int ret;
	
	ret = -1;
	type = ixp_estrdup(address);

	addr = strchr(type, '!');
	if(addr == nil)
		errstr = "no address type defined";
	else {
		*addr++ = '\0';
		if(strcmp(type, "unix") == 0)
			ret = dial_unix(addr);
		else if(strcmp(type, "tcp") == 0)
			ret = dial_tcp(addr);
		else
			errstr = "unkown address type";
	}

	free(type);
	return ret;
}

int
ixp_announce(char *address) {
	char *addr, *type;
	int ret;
	
	ret = -1;
	type = ixp_estrdup(address);

	addr = strchr(type, '!');
	if(addr == nil)
		errstr = "no address type defined";
	else {
		*addr++ = '\0';
		if(strcmp(type, "unix") == 0)
			ret = announce_unix(addr);
		else if(strcmp(type, "tcp") == 0)
			ret = announce_tcp(addr);
		else
			errstr = "unkown address type";
	}

	free(type);
	return ret;
}
