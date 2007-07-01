#define IXP_NO_P9_
#define IXP_P9_STRUCTS
#include <ixp.h>

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
void initmap(Intmap *m, ulong nhash, void *hash);
void incref_map(Intmap *m);
void decref_map(Intmap *m);
void freemap(Intmap *map, void (*destroy)(void*));
void execmap(Intmap *map, void (*destroy)(void*));
void *lookupkey(Intmap *map, ulong id);
void *insertkey(Intmap *map, ulong id, void *v);
void *deletekey(Intmap *map, ulong id);
int caninsertkey(Intmap *map, ulong id, void *v);

#undef nil
#define nil ((void*)0)
#define USED(v) if(v){}else{}

