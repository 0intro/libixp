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

unsigned int
ixp_tokenize(char **result, unsigned int reslen, char *str, char delim) {
	char *p, *n;
	unsigned int i = 0;

	if(!str)
		return 0;
	for(n = str; *n == delim; n++);
	p = n;
	for(i = 0; *n != 0;) {
		if(i == reslen)
			return i;
		if(*n == delim) {
			*n = 0;
			if(strlen(p))
				result[i++] = p;
			p = ++n;
		} else
			n++;
	}
	if((i < reslen) && (p < n) && strlen(p))
		result[i++] = p;
	return i;	/* number of tokens */
}
