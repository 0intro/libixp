#define IXP_NO_P9_
#define IXP_P9_STRUCTS
#include <ixp.h>

char *argv0;
#define ARGBEGIN int _argi, _argtmp, _inargv=0; char *_argv; \
		if(!argv0)argv0=ARGF(); _inargv=1; \
		while(argc && argv[0][0] == '-') { \
			_argi=1; _argv=*argv++; argc--; \
			while(_argv[_argi]) switch(_argv[_argi++])
#define ARGEND }_inargv=0;USED(_argtmp);USED(_argv);USED(_argi)
#define ARGF() ((_inargv && _argv[_argi]) ? \
		(_argtmp=_argi, _argi=strlen(_argv), _argv+_argtmp) \
		: ((argc > 0) ? (argc--, *argv++) : ((char*)0)))
#define EARGF(f) ((_inargv && _argv[_argi]) ? \
		(_argtmp=_argi, _argi=strlen(_argv), _argv+_argtmp) \
		: ((argc > 0) ? (argc--, *argv++) : ((f), (char*)0)))
#define USED(x) if(x){}else
#define SET(x) ((x)=0)

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

void muxinit(IxpClient*);
void muxfree(IxpClient*);
Fcall *muxrpc(IxpClient*, Fcall*);

#define errstr ixp_errstr
#define rerrstr ixp_rerrstr
#define werrstr ixp_werrstr

typedef struct Intlist Intlist;
struct Intmap {
	ulong nhash;
	Intlist	**hash;
	IxpRWLock lk;
};

#define initmap ixp_initmap
#define incref ixp_incref
#define decref ixp_decref
#define freemap ixp_freemap
#define execmap ixp_execmap
#define lookupkey ixp_lookupkey
#define insertkey ixp_insertkey
#define deletekey ixp_deletekey
#define caninsertkey ixp_caninsertkey

/* intmap.c */
void initmap(Intmap*, ulong, void*);
void incref_map(Intmap*);
void decref_map(Intmap*);
void freemap(Intmap*, void (*destroy)(void*));
void execmap(Intmap*, void (*destroy)(void*));
void *lookupkey(Intmap*, ulong);
void *insertkey(Intmap*, ulong, void*);
void *deletekey(Intmap*, ulong);
int caninsertkey(Intmap*, ulong, void*);

#undef nil
#define nil ((void*)0)

