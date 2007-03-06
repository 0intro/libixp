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

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
unsigned int
ixp_strlcat(char *dst, const char *src, unsigned int siz)
{
        const char *s;
        char *d;
        unsigned int n, dlen;

	n = siz;
	s = src;
	d = dst;
        
        /* Find the end of dst and adjust bytes left but don't go past end */
        while (n-- != 0 && *d != '\0')
                d++;
        dlen = d - dst;
        n = siz - dlen;
        
        if (n == 0)
                return(dlen + strlen(s));
        while (*s != '\0') {
                if (n != 1) {
                        *d++ = *s;
                        n--;
                }
                s++;
        }
        *d = '\0';
        
        return(dlen + (s - src));       /* count does not include NUL */
}

