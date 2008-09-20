#define IXP_NO_P9_
#define IXP_P9_STRUCTS
#include <ixp.h>

char *argv0;
#define ARGBEGIN \
		int _argtmp=0, _inargv=0; char *_argv=nil; \
		if(!argv0) {argv0=*argv; argv++, argc--;} \
		_inargv=1; USED(_inargv); \
		while(argc && argv[0][0] == '-') { \
			_argv=&argv[0][1]; argv++; argc--; \
			if(_argv[0] == '-' && _argv[1] == '\0') \
				break; \
			while(*_argv) switch(*_argv++)
#define ARGEND }_inargv=0;USED(_argtmp, _argv, _inargv)

#define EARGF(f) ((_inargv && *_argv) ? \
			(_argtmp=strlen(_argv), _argv+=_argtmp, _argv-_argtmp) \
			: ((argc > 0) ? \
				(--argc, ++argv, _used(argc), *(argv-1)) \
				: ((f), (char*)0)))
#define ARGF() EARGF(_used(0))

#ifndef KENC
  static inline void _used(long a, ...) { if(a){} }
# define USED(...) _used((long)__VA_ARGS__)
# define SET(x) (x = 0)
/* # define SET(x) USED(&x) GCC 4 is 'too smart' for this. */
#endif

#undef nil
#define nil ((void*)0)

#define thread ixp_thread

#define eprint ixp_eprint
#define emalloc ixp_emalloc
#define emallocz ixp_emallocz
#define estrdup ixp_estrdup
#define erealloc ixp_erealloc
#define strlcat ixp_strlcat
#define tokenize ixp_tokenize

#define muxinit ixp_muxinit
#define muxfree ixp_muxfree
#define muxrpc ixp_muxrpc

#define initmap ixp_initmap
#define incref ixp_incref
#define decref ixp_decref
#define freemap ixp_freemap
#define execmap ixp_execmap
#define lookupkey ixp_lookupkey
#define insertkey ixp_insertkey
#define deletekey ixp_deletekey
#define caninsertkey ixp_caninsertkey

#define errstr ixp_errstr
#define rerrstr ixp_rerrstr
#define werrstr ixp_werrstr

typedef struct Intlist Intlist;
typedef IxpTimer Timer;

typedef struct timeval timeval;

struct Intmap {
	ulong nhash;
	Intlist	**hash;
	IxpRWLock lk;
};

struct IxpTimer {
	Timer*	link;
	long	msec;
	long	id;
	void	(*fn)(long, void*);
	void*	aux;
};

/* intmap.c */
int	caninsertkey(Intmap*, ulong, void*);
void	decref_map(Intmap*);
void*	deletekey(Intmap*, ulong);
void	execmap(Intmap*, void (*destroy)(void*));
void	freemap(Intmap*, void (*destroy)(void*));
void	incref_map(Intmap*);
void	initmap(Intmap*, ulong, void*);
void*	insertkey(Intmap*, ulong, void*);
void*	lookupkey(Intmap*, ulong);

/* mux.c */
void	muxfree(IxpClient*);
void	muxinit(IxpClient*);
Fcall*	muxrpc(IxpClient*, Fcall*);

/* timer.c */
long	ixp_nexttimer(IxpServer*);

