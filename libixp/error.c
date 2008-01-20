/* Public Domain --Kris Maglione */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ixp_local.h"

static int
_vsnprint(char *buf, int n, const char *fmt, va_list ap) {
	return vsnprintf(buf, n, fmt, ap);
}

static char*
_vsmprint(const char *fmt, va_list ap) {
	va_list al;
	char *buf = "";
	int n;

	va_copy(al, ap);
	n = vsnprintf(buf, 0, fmt, al);
	va_end(al);

	buf = malloc(++n);
	if(buf)
		vsnprintf(buf, n, fmt, ap);
	return buf;
}

int (*ixp_vsnprint)(char*, int, const char*, va_list) = _vsnprint;
char* (*ixp_vsmprint)(const char*, va_list) = _vsmprint;

/* Approach to errno handling taken from Plan 9 Port. */
enum {
	EPLAN9 = 0x19283745,
};

char*
ixp_errbuf() {
	char *errbuf;

	errbuf = thread->errbuf();
	if(errno == EINTR)
		strncpy(errbuf, "interrupted", IXP_ERRMAX);
	else if(errno != EPLAN9)
		strncpy(errbuf, strerror(errno), IXP_ERRMAX);
	return errbuf;
}

void
errstr(char *buf, int n) {
	char tmp[IXP_ERRMAX];

	strncpy(tmp, buf, sizeof(tmp));
	rerrstr(buf, n);
	strncpy(thread->errbuf(), tmp, IXP_ERRMAX);
	errno = EPLAN9;
}

void
rerrstr(char *buf, int n) {
	strncpy(buf, ixp_errbuf(), n);
}

void
werrstr(const char *fmt, ...) {
	char tmp[IXP_ERRMAX];
	va_list ap;

	va_start(ap, fmt);
	ixp_vsnprint(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	strncpy(thread->errbuf(), tmp, IXP_ERRMAX);
	errno = EPLAN9;
}

