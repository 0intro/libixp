#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "ixp_local.h"

int (*ixp_vsnprint)(char*, int, char*, va_list);

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
werrstr(char *fmt, ...) {
	char tmp[IXP_ERRMAX];
	va_list ap;

	va_start(ap, fmt);
	if(ixp_vsnprint)
		ixp_vsnprint(tmp, sizeof(tmp), fmt, ap);
	else
		vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	strncpy(thread->errbuf(), tmp, IXP_ERRMAX);
	errno = EPLAN9;
}

