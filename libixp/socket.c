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
#include "ixp_local.h"

/* Note: These functions modify the strings that they are passed.
 *   The lookup function duplicates the original string, so it is
 *   not modified.
 */

typedef struct sockaddr sockaddr;
typedef struct sockaddr_un sockaddr_un;
typedef struct sockaddr_in sockaddr_in;

static int
get_port(char *addr) {
	char *s, *end;
	int port;

	s = strchr(addr, '!');
	if(s == nil) {
		werrstr("no port provided");
		return -1;
	}

	*s++ = '\0';
	port = strtol(s, &end, 10);
	if(*s == '\0' && *end != '\0') {
		werrstr("invalid port number");
		return -1;
	}
	return port;
}

static int
sock_unix(char *address, sockaddr_un *sa, socklen_t *salen) {
	int fd;

	memset(sa, 0, sizeof(sa));

	sa->sun_family = AF_UNIX;
	strncpy(sa->sun_path, address, sizeof(sa->sun_path));
	*salen = SUN_LEN(sa);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd < 0)
		return -1;
	return fd;
}

static int
sock_tcp(char *host, sockaddr_in *sa) {
	struct hostent *he;
	int port, fd;

	/* Truncates host at '!' */
	port = get_port(host);
	if(port < 0)
		return -1;

	signal(SIGPIPE, SIG_IGN);
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0)
		return -1;

	memset(sa, 0, sizeof(sa));
	sa->sin_family = AF_INET;
	sa->sin_port = htons(port);

	if(strcmp(host, "*") == 0)
		sa->sin_addr.s_addr = htonl(INADDR_ANY);
	else if((he = gethostbyname(host)))
		memcpy(&sa->sin_addr, he->h_addr, he->h_length);
	else
		return -1;
	return fd;
}

static int
dial_unix(char *address) {
	sockaddr_un sa;
	socklen_t salen;
	int fd;

	fd = sock_unix(address, &sa, &salen);
	if(fd == -1)
		return fd;

	if(connect(fd, (sockaddr*) &sa, salen)) {
		close(fd);
		return -1;
	}
	return fd;
}

static int
announce_unix(char *file) {
	const int yes = 1;
	sockaddr_un sa;
	socklen_t salen;
	int fd;

	signal(SIGPIPE, SIG_IGN);

	fd = sock_unix(file, &sa, &salen);
	if(fd == -1)
		return fd;

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(yes)) < 0)
		goto fail;

	unlink(file);
	if(bind(fd, (sockaddr*)&sa, salen) < 0)
		goto fail;

	chmod(file, S_IRWXU);
	if(listen(fd, IXP_MAX_CACHE) < 0)
		goto fail;

	return fd;

fail:
	close(fd);
	return -1;
}

static int
dial_tcp(char *host) {
	sockaddr_in sa;
	int fd;

	fd = sock_tcp(host, &sa);
	if(fd == -1)
		return fd;

	if(connect(fd, (sockaddr*)&sa, sizeof(sa))) {
		close(fd);
		return -1;
	}

	return fd;
}

static int
announce_tcp(char *host) {
	sockaddr_in sa;
	int fd;

	fd = sock_tcp(host, &sa);
	if(fd == -1)
		return fd;

	if(bind(fd, (sockaddr*)&sa, sizeof(sa)) < 0)
		goto fail;

	if(listen(fd, IXP_MAX_CACHE) < 0)
		goto fail;

	return fd;

fail:
	close(fd);
	return -1;
}

typedef struct addrtab addrtab;
struct addrtab {
	char *type;
	int (*fn)(char*);
} dtab[] = {
	{"tcp", dial_tcp},
	{"unix", dial_unix},
	{0, 0}
}, atab[] = {
	{"tcp", announce_tcp},
	{"unix", announce_unix},
	{0, 0}
};

static int
lookup(char *address, addrtab *tab) {
	char *addr, *type;
	int ret;

	ret = -1;
	type = estrdup(address);

	addr = strchr(type, '!');
	if(addr == nil)
		werrstr("no address type defined");
	else {
		*addr++ = '\0';
		for(; tab->type; tab++)
			if(strcmp(tab->type, type) == 0) break;
		if(tab->type == nil)
			werrstr("unsupported address type");
		else
			ret = tab->fn(addr);
	}

	free(type);
	return ret;
}

int
ixp_dial(char *address) {
	return lookup(address, dtab);
}

int
ixp_announce(char *address) {
	return lookup(address, atab);
}
