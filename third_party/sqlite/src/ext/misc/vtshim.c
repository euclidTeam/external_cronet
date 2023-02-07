/*
** 2013-06-12
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** A shim that sits between the SQLite virtual table interface and
** runtimes with garbage collector based memory management.
*/
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <assert.h>
#include <string.h>

#ifndef SQLITE_OMIT_VIRTUALTABLE

/* Forward references */
typedef struct vtshim_aux vtshim_aux;
typedef struct vtshim_vtab vtshim_vtab;
typedef struct vtshim_cursor vtshim_cursor;


/* The vtshim_aux argument is the auxiliary parameter that is passed
** into sqlite3_create_module_v2().
*/
struct vtshim_aux {
  void *pChildAux;              /* pAux for child virtual tables */
  void (*xChildDestroy)(void*); /* Destructor for pChildAux */
  sqlite3_module *pMod;         /* Methods for child virtual tables */
  sqlite3 *db;                  /* The database to which we are attached */
  char *zName;                  /* Name of the module */
  int bDisposed;                /* True if disposed */
  vtshim_vtab *pAllVtab;        /* List of all vtshim_vtab objects */
  sqlite3_module sSelf;         /* Methods used by this shim */
};

/* A vtshim virtual table object */
struct vtshim_vtab {
  sqlite3_vtab base;       /* Base class - must be first */
  sqlite3_vtab *pChild;    /* Child virtual table */
  vtshim_aux *pAux;        /* Pointer to vtshim_aux object */
  vtshim_cursor *pAllCur;  /* List of all cursors */
  vtshim_vtab **ppPrev;    /* Previous on list */
  vtshim_vtab *pNext;      /* Next on list */
};

/* A vtshim cursor object */
struct vtshim_cursor {
  sqlite3_vtab_cursor base;    /* Base class - must be first */
  sqlite3_vtab_cursor *pChild; /* Cursor generated by the managed subclass */
  vtshim_cursor **ppPrev;      /* Previous on list of all cursors */
  vtshim_cursor *pNext;        /* Next on list of all cursors */
};

/* Macro used to copy the child vtable error message to outer vtable */
#define VTSHIM_COPY_ERRMSG()                                             \
  do {                                                                   \
    sqlite3_free(pVtab->base.zErrMsg);                                   \
    pVtab->base.zErrMsg = sqlite3_mprintf("%s", pVtab->pChild->zErrMsg); \
  } while (0)

/* Methods for the vtshim module */
static int vtshimCreate(
  sqlite3 *db,
  void *ppAux,
  int argc,
  const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  vtshim_aux *pAux = (vtshim_aux*)ppAux;
  vtshim_vtab *pNew;
  int rc;

  assert( db==pAux->db );
  if( pAux->bDisposed ){
    if( pzErr ){
      *pzErr = sqlite3_mprintf("virtual table was disposed: \"%s\"",
                               pAux->zName);
    }
    return SQLITE_ERROR;
  }
  pNew = sqlite3_malloc( sizeof(*pNew) );
  *ppVtab = (sqlite3_vtab*)pNew;
  if( pNew==0 ) return SQLITE_NOMEM;
  memset(pNew, 0, sizeof(*pNew));
  rc = pAux->pMod->xCreate(db, pAux->pChildAux, argc, argv,
                           &pNew->pChild, pzErr);
  if( rc ){
    sqlite3_free(pNew);
    *ppVtab = 0;
    return rc;
  }
  pNew->pAux = pAux;
  pNew->ppPrev = &pAux->pAllVtab;
  pNew->pNext = pAux->pAllVtab;
  if( pAux->pAllVtab ) pAux->pAllVtab->ppPrev = &pNew->pNext;
  pAux->pAllVtab = pNew;
  return rc;
}

static int vtshimConnect(
  sqlite3 *db,
  void *ppAux,
  int argc,
  const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  vtshim_aux *pAux = (vtshim_aux*)ppAux;
  vtshim_vtab *pNew;
  int rc;

  assert( db==pAux->db );
  if( pAux->bDisposed ){
    if( pzErr ){
      *pzErr = sqlite3_mprintf("virtual table was disposed: \"%s\"",
                               pAux->zName);
    }
    return SQLITE_ERROR;
  }
  pNew = sqlite3_malloc( sizeof(*pNew) );
  *ppVtab = (sqlite3_vtab*)pNew;
  if( pNew==0 ) return SQLITE_NOMEM;
  memset(pNew, 0, sizeof(*pNew));
  rc = pAux->pMod->xConnect(db, pAux->pChildAux, argc, argv,
                            &pNew->pChild, pzErr);
  if( rc ){
    sqlite3_free(pNew);
    *ppVtab = 0;
    return rc;
  }
  pNew->pAux = pAux;
  pNew->ppPrev = &pAux->pAllVtab;
  pNew->pNext = pAux->pAllVtab;
  if( pAux->pAllVtab ) pAux->pAllVtab->ppPrev = &pNew->pNext;
  pAux->pAllVtab = pNew;
  return rc;
}

static int vtshimBestIndex(
  sqlite3_vtab *pBase,
  sqlite3_index_info *pIdxInfo
){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xBestIndex(pVtab->pChild, pIdxInfo);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimDisconnect(sqlite3_vtab *pBase){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc = SQLITE_OK;
  if( !pAux->bDisposed ){
    rc = pAux->pMod->xDisconnect(pVtab->pChild);
  }
  if( pVtab->pNext ) pVtab->pNext->ppPrev = pVtab->ppPrev;
  *pVtab->ppPrev = pVtab->pNext;
  sqlite3_free(pVtab);
  return rc;
}

static int vtshimDestroy(sqlite3_vtab *pBase){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc = SQLITE_OK;
  if( !pAux->bDisposed ){
    rc = pAux->pMod->xDestroy(pVtab->pChild);
  }
  if( pVtab->pNext ) pVtab->pNext->ppPrev = pVtab->ppPrev;
  *pVtab->ppPrev = pVtab->pNext;
  sqlite3_free(pVtab);
  return rc;
}

static int vtshimOpen(sqlite3_vtab *pBase, sqlite3_vtab_cursor **ppCursor){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  vtshim_cursor *pCur;
  int rc;
  *ppCursor = 0;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  pCur = sqlite3_malloc( sizeof(*pCur) );
  if( pCur==0 ) return SQLITE_NOMEM;
  memset(pCur, 0, sizeof(*pCur));
  rc = pAux->pMod->xOpen(pVtab->pChild, &pCur->pChild);
  if( rc ){
    sqlite3_free(pCur);
    VTSHIM_COPY_ERRMSG();
    return rc;
  }
  pCur->pChild->pVtab = pVtab->pChild;
  *ppCursor = &pCur->base;
  pCur->ppPrev = &pVtab->pAllCur;
  if( pVtab->pAllCur ) pVtab->pAllCur->ppPrev = &pCur->pNext;
  pCur->pNext = pVtab->pAllCur;
  pVtab->pAllCur = pCur;
  return SQLITE_OK;
}

static int vtshimClose(sqlite3_vtab_cursor *pX){
  vtshim_cursor *pCur = (vtshim_cursor*)pX;
  vtshim_vtab *pVtab = (vtshim_vtab*)pCur->base.pVtab;
  vtshim_aux *pAux = pVtab->pAux;
  int rc = SQLITE_OK;
  if( !pAux->bDisposed ){
    rc = pAux->pMod->xClose(pCur->pChild);
    if( rc!=SQLITE_OK ){
      VTSHIM_COPY_ERRMSG();
    }
  }
  if( pCur->pNext ) pCur->pNext->ppPrev = pCur->ppPrev;
  *pCur->ppPrev = pCur->pNext;
  sqlite3_free(pCur);
  return rc;
}

static int vtshimFilter(
  sqlite3_vtab_cursor *pX,
  int idxNum,
  const char *idxStr,
  int argc,
  sqlite3_value **argv
){
  vtshim_cursor *pCur = (vtshim_cursor*)pX;
  vtshim_vtab *pVtab = (vtshim_vtab*)pCur->base.pVtab;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xFilter(pCur->pChild, idxNum, idxStr, argc, argv);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimNext(sqlite3_vtab_cursor *pX){
  vtshim_cursor *pCur = (vtshim_cursor*)pX;
  vtshim_vtab *pVtab = (vtshim_vtab*)pCur->base.pVtab;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xNext(pCur->pChild);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimEof(sqlite3_vtab_cursor *pX){
  vtshim_cursor *pCur = (vtshim_cursor*)pX;
  vtshim_vtab *pVtab = (vtshim_vtab*)pCur->base.pVtab;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return 1;
  rc = pAux->pMod->xEof(pCur->pChild);
  VTSHIM_COPY_ERRMSG();
  return rc;
}

static int vtshimColumn(sqlite3_vtab_cursor *pX, sqlite3_context *ctx, int i){
  vtshim_cursor *pCur = (vtshim_cursor*)pX;
  vtshim_vtab *pVtab = (vtshim_vtab*)pCur->base.pVtab;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xColumn(pCur->pChild, ctx, i);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimRowid(sqlite3_vtab_cursor *pX, sqlite3_int64 *pRowid){
  vtshim_cursor *pCur = (vtshim_cursor*)pX;
  vtshim_vtab *pVtab = (vtshim_vtab*)pCur->base.pVtab;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xRowid(pCur->pChild, pRowid);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimUpdate(
  sqlite3_vtab *pBase,
  int argc,
  sqlite3_value **argv,
  sqlite3_int64 *pRowid
){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xUpdate(pVtab->pChild, argc, argv, pRowid);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimBegin(sqlite3_vtab *pBase){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xBegin(pVtab->pChild);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimSync(sqlite3_vtab *pBase){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xSync(pVtab->pChild);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimCommit(sqlite3_vtab *pBase){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xCommit(pVtab->pChild);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimRollback(sqlite3_vtab *pBase){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xRollback(pVtab->pChild);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimFindFunction(
  sqlite3_vtab *pBase,
  int nArg,
  const char *zName,
  void (**pxFunc)(sqlite3_context*,int,sqlite3_value**),
  void **ppArg
){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return 0;
  rc = pAux->pMod->xFindFunction(pVtab->pChild, nArg, zName, pxFunc, ppArg);
  VTSHIM_COPY_ERRMSG();
  return rc;
}

static int vtshimRename(sqlite3_vtab *pBase, const char *zNewName){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xRename(pVtab->pChild, zNewName);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimSavepoint(sqlite3_vtab *pBase, int n){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xSavepoint(pVtab->pChild, n);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimRelease(sqlite3_vtab *pBase, int n){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xRelease(pVtab->pChild, n);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

static int vtshimRollbackTo(sqlite3_vtab *pBase, int n){
  vtshim_vtab *pVtab = (vtshim_vtab*)pBase;
  vtshim_aux *pAux = pVtab->pAux;
  int rc;
  if( pAux->bDisposed ) return SQLITE_ERROR;
  rc = pAux->pMod->xRollbackTo(pVtab->pChild, n);
  if( rc!=SQLITE_OK ){
    VTSHIM_COPY_ERRMSG();
  }
  return rc;
}

/* The destructor function for a disposible module */
static void vtshimAuxDestructor(void *pXAux){
  vtshim_aux *pAux = (vtshim_aux*)pXAux;
  assert( pAux->pAllVtab==0 );
  if( !pAux->bDisposed && pAux->xChildDestroy ){
    pAux->xChildDestroy(pAux->pChildAux);
    pAux->xChildDestroy = 0;
  }
  sqlite3_free(pAux->zName);
  sqlite3_free(pAux->pMod);
  sqlite3_free(pAux);
}

static int vtshimCopyModule(
  const sqlite3_module *pMod,   /* Source module to be copied */
  sqlite3_module **ppMod        /* Destination for copied module */
){
  sqlite3_module *p;
  if( !pMod || !ppMod ) return SQLITE_ERROR;
  p = sqlite3_malloc( sizeof(*p) );
  if( p==0 ) return SQLITE_NOMEM;
  memcpy(p, pMod, sizeof(*p));
  *ppMod = p;
  return SQLITE_OK;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void *sqlite3_create_disposable_module(
  sqlite3 *db,               /* SQLite connection to register module with */
  const char *zName,         /* Name of the module */
  const sqlite3_module *p,   /* Methods for the module */
  void *pClientData,         /* Client data for xCreate/xConnect */
  void(*xDestroy)(void*)     /* Module destructor function */
){
  vtshim_aux *pAux;
  sqlite3_module *pMod;
  int rc;
  pAux = sqlite3_malloc( sizeof(*pAux) );
  if( pAux==0 ){
    if( xDestroy ) xDestroy(pClientData);
    return 0;
  }
  rc = vtshimCopyModule(p, &pMod);
  if( rc!=SQLITE_OK ){
    sqlite3_free(pAux);
    return 0;
  }
  pAux->pChildAux = pClientData;
  pAux->xChildDestroy = xDestroy;
  pAux->pMod = pMod;
  pAux->db = db;
  pAux->zName = sqlite3_mprintf("%s", zName);
  pAux->bDisposed = 0;
  pAux->pAllVtab = 0;
  pAux->sSelf.iVersion = p->iVersion<=2 ? p->iVersion : 2;
  pAux->sSelf.xCreate = p->xCreate ? vtshimCreate : 0;
  pAux->sSelf.xConnect = p->xConnect ? vtshimConnect : 0;
  pAux->sSelf.xBestIndex = p->xBestIndex ? vtshimBestIndex : 0;
  pAux->sSelf.xDisconnect = p->xDisconnect ? vtshimDisconnect : 0;
  pAux->sSelf.xDestroy = p->xDestroy ? vtshimDestroy : 0;
  pAux->sSelf.xOpen = p->xOpen ? vtshimOpen : 0;
  pAux->sSelf.xClose = p->xClose ? vtshimClose : 0;
  pAux->sSelf.xFilter = p->xFilter ? vtshimFilter : 0;
  pAux->sSelf.xNext = p->xNext ? vtshimNext : 0;
  pAux->sSelf.xEof = p->xEof ? vtshimEof : 0;
  pAux->sSelf.xColumn = p->xColumn ? vtshimColumn : 0;
  pAux->sSelf.xRowid = p->xRowid ? vtshimRowid : 0;
  pAux->sSelf.xUpdate = p->xUpdate ? vtshimUpdate : 0;
  pAux->sSelf.xBegin = p->xBegin ? vtshimBegin : 0;
  pAux->sSelf.xSync = p->xSync ? vtshimSync : 0;
  pAux->sSelf.xCommit = p->xCommit ? vtshimCommit : 0;
  pAux->sSelf.xRollback = p->xRollback ? vtshimRollback : 0;
  pAux->sSelf.xFindFunction = p->xFindFunction ? vtshimFindFunction : 0;
  pAux->sSelf.xRename = p->xRename ? vtshimRename : 0;
  if( p->iVersion>=2 ){
    pAux->sSelf.xSavepoint = p->xSavepoint ? vtshimSavepoint : 0;
    pAux->sSelf.xRelease = p->xRelease ? vtshimRelease : 0;
    pAux->sSelf.xRollbackTo = p->xRollbackTo ? vtshimRollbackTo : 0;
  }else{
    pAux->sSelf.xSavepoint = 0;
    pAux->sSelf.xRelease = 0;
    pAux->sSelf.xRollbackTo = 0;
  }
  rc = sqlite3_create_module_v2(db, zName, &pAux->sSelf,
                                pAux, vtshimAuxDestructor);
  return rc==SQLITE_OK ? (void*)pAux : 0;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void sqlite3_dispose_module(void *pX){
  vtshim_aux *pAux = (vtshim_aux*)pX;
  if( !pAux->bDisposed ){
    vtshim_vtab *pVtab;
    vtshim_cursor *pCur;
    for(pVtab=pAux->pAllVtab; pVtab; pVtab=pVtab->pNext){
      for(pCur=pVtab->pAllCur; pCur; pCur=pCur->pNext){
        pAux->pMod->xClose(pCur->pChild);
      }
      pAux->pMod->xDisconnect(pVtab->pChild);
    }
    pAux->bDisposed = 1;
    if( pAux->xChildDestroy ){
      pAux->xChildDestroy(pAux->pChildAux);
      pAux->xChildDestroy = 0;
    }
  }
}


#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_vtshim_init(
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  SQLITE_EXTENSION_INIT2(pApi);
  return SQLITE_OK;
}
