/* (C)opyright MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "ixp.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void *
ixp_emalloc(unsigned int size) {
	void *res = malloc(size);

	if(!res)
		ixp_eprint("fatal: could not malloc() %u bytes\n", size);
	return res;
}

void *
ixp_emallocz(unsigned int size) {
	void *res = calloc(1, size);

	if(!res)
		ixp_eprint("fatal: could not malloc() %u bytes\n", size);
	return res;
}

void
ixp_eprint(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void *
ixp_erealloc(void *ptr, unsigned int size) {
	void *res = realloc(ptr, size);

	if(!res)
		ixp_eprint("fatal: could not malloc() %u bytes\n", size);
	return res;
}

char *
ixp_estrdup(const char *str) {
	void *res = strdup(str);

	if(!res)
		ixp_eprint("fatal: could not malloc() %u bytes\n", strlen(str));
	return res;
}
