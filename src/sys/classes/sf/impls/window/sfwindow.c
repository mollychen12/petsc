#include <petsc-private/sfimpl.h> /*I "petscsf.h" I*/

typedef struct _n_PetscSFDataLink *PetscSFDataLink;
typedef struct _n_PetscSFWinLink  *PetscSFWinLink;

#if !defined(PETSC_HAVE_MPI_WIN_CREATE) /* The intent here is to be able to compile even without a complete MPI. */
typedef struct MPI_Win_MISSING *MPI_Win;
#endif

typedef struct {
  PetscSFWindowSyncType sync; /* FENCE, LOCK, or ACTIVE synchronization */
  PetscSFDataLink link;         /* List of MPI data types and windows, lazily constructed for each data type */
  PetscSFWinLink  wins;         /* List of active windows */
} PetscSF_Window;

struct _n_PetscSFDataLink {
  MPI_Datatype unit;
  MPI_Datatype *mine;
  MPI_Datatype *remote;
  PetscSFDataLink next;
};

struct _n_PetscSFWinLink {
  PetscBool      inuse;
  size_t         bytes;
  void           *addr;
  MPI_Win        win;
  PetscBool      epoch;
  PetscSFWinLink next;
};

const char *const PetscSFWindowSyncTypes[] = {"FENCE","LOCK","ACTIVE","PetscSFWindowSyncType","PETSCSF_WINDOW_SYNC_",0};

#if !defined(PETSC_HAVE_MPI_WIN_CREATE)
#define MPI_WIN_NULL ((MPI_Win)0)
#define MPI_REPLACE 0
#define MPI_Win_create(base,size,disp_unit,info,comm,win) 1;SETERRQ(comm,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Win_free(win) 1;SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Win_start(group,assert,win) 1;SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Win_post(group,assert,win) 1;SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Win_complete(win) 1;SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Win_wait(win) 1;SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Win_lock(lock_type,rank,assert,win) 1;SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Win_unlock(rank,win) 1;SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Win_fence(assert,win) 1;SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Get(origin_addr,origin_count,origin_datatype,target_rank,target_displ,target_count,target_datatype,win) 1;SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Accumulate(origin_addr,origin_count,origin_datatype,target_rank,target_displ,target_count,target_datatype,op,win) 1;SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_MODE_NOPUT     0
#define MPI_MODE_NOPRECEDE 0
#define MPI_MODE_NOSUCCEED 0
#define MPI_MODE_NOSTORE   0
#endif

#if !defined(PETSC_HAVE_MPI_TYPE_DUP)
#define MPI_Type_dup(datatype,newtype) (*(newtype)=0,1);SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Type_create_indexed_block(count,blocklength,displs,oldtype,newtype) (*(newtype)=0,1);SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#define MPI_Type_get_true_extent(type,lb,bytes) (*(lb)=0,*(bytes)=0,1);SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#endif
#if !defined(PETSC_HAVE_MPI_TYPE_GET_EXTENT) && !defined(PETSC_HAVE_MPIUNI) /* MPIUNI defines this, although it is not in the library */
#define MPI_Type_get_extent(type,lb,bytes) (*(lb)=0,*(bytes)=0,1);SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP_SYS,"Need an MPI-2 implementation")
#endif

#undef __FUNCT__
#define __FUNCT__ "PetscSFWindowOpTranslate"
/* Built-in MPI_Ops act elementwise inside MPI_Accumulate, but cannot be used with composite types inside collectives (MPI_Allreduce) */
static PetscErrorCode PetscSFWindowOpTranslate(MPI_Op *op)
{

  PetscFunctionBegin;
  if (*op == MPIU_SUM) *op = MPI_SUM;
  else if (*op == MPIU_MAX) *op = MPI_MAX;
  else if (*op == MPIU_MIN) *op = MPI_MIN;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFWindowGetDataTypes"
/*@C
   PetscSFWindowGetDataTypes - gets composite local and remote data types for each rank

   Not Collective

   Input Arguments:
+  sf - star forest
-  unit - data type for each node

   Output Arguments:
+  localtypes - types describing part of local leaf buffer referencing each remote rank
-  remotetypes - types describing part of remote root buffer referenced for each remote rank

   Level: developer

.seealso: PetscSFSetGraph(), PetscSFView()
@*/
static PetscErrorCode PetscSFWindowGetDataTypes(PetscSF sf,MPI_Datatype unit,const MPI_Datatype **localtypes,const MPI_Datatype **remotetypes)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;
  PetscErrorCode ierr;
  PetscSFDataLink link;
  PetscInt i,nranks;
  const PetscInt *roffset,*rmine,*rremote;
  const PetscMPIInt *ranks;

  PetscFunctionBegin;
  /* Look for types in cache */
  for (link=w->link; link; link=link->next) {
    PetscBool match;
    ierr = MPIPetsc_Type_compare(unit,link->unit,&match);CHKERRQ(ierr);
    if (match) {
      *localtypes = link->mine;
      *remotetypes = link->remote;
      PetscFunctionReturn(0);
    }
  }

  /* Create new composite types for each send rank */
  ierr = PetscSFGetRanks(sf,&nranks,&ranks,&roffset,&rmine,&rremote);CHKERRQ(ierr);
  ierr = PetscMalloc(sizeof(*link),&link);CHKERRQ(ierr);
  ierr = MPI_Type_dup(unit,&link->unit);CHKERRQ(ierr);
  ierr = PetscMalloc2(nranks,MPI_Datatype,&link->mine,nranks,MPI_Datatype,&link->remote);CHKERRQ(ierr);
  for (i=0; i<nranks; i++) {
    PETSC_UNUSED PetscInt rcount = roffset[i+1] - roffset[i];
    PetscMPIInt *rmine,*rremote;
#if !defined(PETSC_USE_64BIT_INDICES)
    rmine   = sf->rmine + sf->roffset[i];
    rremote = sf->rremote + sf->roffset[i];
#else
    PetscInt j;
    ierr = PetscMalloc2(rcount,PetscMPIInt,&rmine,rcount,PetscMPIInt,&rremote);CHKERRQ(ierr);
    for (j=0; j<rcount; j++) {
      rmine[j]   = PetscMPIIntCast(sf->rmine[sf->roffset[i]+j]);
      rremote[j] = PetscMPIIntCast(sf->rremote[sf->roffset[i]+j]);
    }
#endif
    ierr = MPI_Type_create_indexed_block(rcount,1,rmine,link->unit,&link->mine[i]);CHKERRQ(ierr);
    ierr = MPI_Type_create_indexed_block(rcount,1,rremote,link->unit,&link->remote[i]);CHKERRQ(ierr);
#if defined(PETSC_USE_64BIT_INDICES)
    ierr = PetscFree2(rmine,rremote);CHKERRQ(ierr);
#endif
    ierr = MPI_Type_commit(&link->mine[i]);CHKERRQ(ierr);
    ierr = MPI_Type_commit(&link->remote[i]);CHKERRQ(ierr);
  }
  link->next = w->link;
  w->link = link;

  *localtypes = link->mine;
  *remotetypes = link->remote;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFWindowSetSyncType"
/*@C
   PetscSFWindowSetSyncType - set synchrozitaion type for PetscSF communication

   Logically Collective

   Input Arguments:
+  sf - star forest for communication
-  sync - synchronization type

   Options Database Key:
.  -sf_synchronization <sync> - sets the synchronization type

   Level: advanced

.seealso: PetscSFSetFromOptions(), PetscSFWindowGetSyncType()
@*/
PetscErrorCode PetscSFWindowSetSyncType(PetscSF sf,PetscSFWindowSyncType sync)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(sf,PETSCSF_CLASSID,1);
  PetscValidLogicalCollectiveEnum(sf,sync,2);
  ierr = PetscUseMethod(sf,"PetscSFWindowSetSyncType_C",(PetscSF,PetscSFWindowSyncType),(sf,sync));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFWindowSetSyncType_Window"
PETSC_EXTERN_C PetscErrorCode PetscSFWindowSetSyncType_Window(PetscSF sf,PetscSFWindowSyncType sync)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;

  PetscFunctionBegin;
  w->sync = sync;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFWindowGetSyncType"
/*@C
   PetscSFWindowGetSyncType - get synchrozitaion type for PetscSF communication

   Logically Collective

   Input Argument:
.  sf - star forest for communication

   Output Argument:
.  sync - synchronization type

   Level: advanced

.seealso: PetscSFGetFromOptions(), PetscSFWindowSetSyncType()
@*/
PetscErrorCode PetscSFWindowGetSyncType(PetscSF sf,PetscSFWindowSyncType *sync)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(sf,PETSCSF_CLASSID,1);
  PetscValidPointer(sync,2);
  ierr = PetscTryMethod(sf,"PetscSFWindowGetSyncType_C",(PetscSF,PetscSFWindowSyncType*),(sf,sync));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFWindowGetSyncType_Window"
PETSC_EXTERN_C PetscErrorCode PetscSFWindowGetSyncType_Window(PetscSF sf,PetscSFWindowSyncType *sync)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;

  PetscFunctionBegin;
  *sync = w->sync;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFGetWindow"
/*@C
   PetscSFGetWindow - Get a window for use with a given data type

   Collective on PetscSF

   Input Arguments:
+  sf - star forest
.  unit - data type
.  array - array to be sent
.  epoch - PETSC_TRUE to acquire the window and start an epoch, PETSC_FALSE to just acquire the window
.  fenceassert - assert parameter for call to MPI_Win_fence(), if PETSCSF_WINDOW_SYNC_FENCE
.  postassert - assert parameter for call to MPI_Win_post(), if PETSCSF_WINDOW_SYNC_ACTIVE
-  startassert - assert parameter for call to MPI_Win_start(), if PETSCSF_WINDOW_SYNC_ACTIVE

   Output Arguments:
.  win - window

   Level: developer

   Developer Notes:
   This currently always creates a new window. This is more synchronous than necessary. An alternative is to try to
   reuse an existing window created with the same array. Another alternative is to maintain a cache of windows and reuse
   whichever one is available, by copying the array into it if necessary.

.seealso: PetscSFGetRanks(), PetscSFWindowGetDataTypes()
@*/
static PetscErrorCode PetscSFGetWindow(PetscSF sf,MPI_Datatype unit,void *array,PetscBool epoch,PetscMPIInt fenceassert,PetscMPIInt postassert,PetscMPIInt startassert,MPI_Win *win)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;
  PetscErrorCode ierr;
  MPI_Aint lb,lb_true,bytes,bytes_true;
  PetscSFWinLink link;

  PetscFunctionBegin;
  ierr = MPI_Type_get_extent(unit,&lb,&bytes);CHKERRQ(ierr);
  ierr = MPI_Type_get_true_extent(unit,&lb_true,&bytes_true);CHKERRQ(ierr);
  if (lb != 0 || lb_true != 0) SETERRQ(((PetscObject)sf)->comm,PETSC_ERR_SUP,"No support for unit type with nonzero lower bound, write petsc-maint@mcs.anl.gov if you want this feature");
  if (bytes != bytes_true) SETERRQ(((PetscObject)sf)->comm,PETSC_ERR_SUP,"No support for unit type with modified extent, write petsc-maint@mcs.anl.gov if you want this feature");
  ierr = PetscMalloc(sizeof(*link),&link);CHKERRQ(ierr);
  link->bytes = bytes;
  link->addr  = array;
  ierr = MPI_Win_create(array,(MPI_Aint)bytes*sf->nroots,(PetscMPIInt)bytes,MPI_INFO_NULL,((PetscObject)sf)->comm,&link->win);CHKERRQ(ierr);
  link->epoch = epoch;
  link->next = w->wins;
  link->inuse = PETSC_TRUE;
  w->wins = link;
  *win = link->win;

  if (epoch) {
    switch (w->sync) {
    case PETSCSF_WINDOW_SYNC_FENCE:
      ierr = MPI_Win_fence(fenceassert,*win);CHKERRQ(ierr);
      break;
    case PETSCSF_WINDOW_SYNC_LOCK: /* Handled outside */
      break;
    case PETSCSF_WINDOW_SYNC_ACTIVE: {
      MPI_Group ingroup,outgroup;
      ierr = PetscSFGetGroups(sf,&ingroup,&outgroup);CHKERRQ(ierr);
      ierr = MPI_Win_post(ingroup,postassert,*win);CHKERRQ(ierr);
      ierr = MPI_Win_start(outgroup,startassert,*win);CHKERRQ(ierr);
    } break;
    default: SETERRQ(((PetscObject)sf)->comm,PETSC_ERR_PLIB,"Unknown synchronization type");
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFFindWindow"
/*@C
   PetscSFFindWindow - Finds a window that is already in use

   Not Collective

   Input Arguments:
+  sf - star forest
.  unit - data type
-  array - array with which the window is associated

   Output Arguments:
.  win - window

   Level: developer

.seealso: PetscSFGetWindow(), PetscSFRestoreWindow()
@*/
static PetscErrorCode PetscSFFindWindow(PetscSF sf,MPI_Datatype unit,const void *array,MPI_Win *win)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;
  PetscSFWinLink link;

  PetscFunctionBegin;
  for (link=w->wins; link; link=link->next) {
    if (array == link->addr) {
      *win = link->win;
      PetscFunctionReturn(0);
    }
  }
  SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Requested window not in use");
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFRestoreWindow"
/*@C
   PetscSFRestoreWindow - Restores a window obtained with PetscSFGetWindow()

   Collective

   Input Arguments:
+  sf - star forest
.  unit - data type
.  array - array associated with window
.  epoch - close an epoch, must match argument to PetscSFGetWindow()
-  win - window

   Level: developer

.seealso: PetscSFFindWindow()
@*/
static PetscErrorCode PetscSFRestoreWindow(PetscSF sf,MPI_Datatype unit,const void *array,PetscBool epoch,PetscMPIInt fenceassert,MPI_Win *win)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;
  PetscErrorCode ierr;
  PetscSFWinLink *p,link;

  PetscFunctionBegin;
  for (p=&w->wins; *p; p=&(*p)->next) {
    link = *p;
    if (*win == link->win) {
      if (array != link->addr) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Matched window, but not array");
      if (epoch != link->epoch) {
        if (epoch) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"No epoch to end");
        else SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Restoring window without ending epoch");
      }
      *p = link->next;
      goto found;
    }
  }
  SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Requested window not in use");

  found:
  if (epoch) {
    switch (w->sync) {
    case PETSCSF_WINDOW_SYNC_FENCE:
      ierr = MPI_Win_fence(fenceassert,*win);CHKERRQ(ierr);
      break;
    case PETSCSF_WINDOW_SYNC_LOCK:
      break;                    /* handled outside */
    case PETSCSF_WINDOW_SYNC_ACTIVE: {
      ierr = MPI_Win_complete(*win);CHKERRQ(ierr);
      ierr = MPI_Win_wait(*win);CHKERRQ(ierr);
    } break;
    default: SETERRQ(((PetscObject)sf)->comm,PETSC_ERR_PLIB,"Unknown synchronization type");
    }
  }

  ierr = MPI_Win_free(&link->win);CHKERRQ(ierr);
  ierr = PetscFree(link);CHKERRQ(ierr);
  *win = MPI_WIN_NULL;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFSetFromOptions_Window"
static PetscErrorCode PetscSFSetFromOptions_Window(PetscSF sf)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscOptionsHead("PetscSF Window options");CHKERRQ(ierr);
  ierr = PetscOptionsEnum("-sf_window_sync","synchronization type to use for PetscSF Window communication","PetscSFWindowSetSyncType",PetscSFWindowSyncTypes,(PetscEnum)w->sync,(PetscEnum*)&w->sync,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFReset_Window"
static PetscErrorCode PetscSFReset_Window(PetscSF sf)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;
  PetscErrorCode ierr;
  PetscSFDataLink link,next;
  PetscSFWinLink  wlink,wnext;
  PetscInt i;

  PetscFunctionBegin;
  for (link=w->link; link; link=next) {
    next = link->next;
    ierr = MPI_Type_free(&link->unit);CHKERRQ(ierr);
    for (i=0; i<sf->nranks; i++) {
      ierr = MPI_Type_free(&link->mine[i]);CHKERRQ(ierr);
      ierr = MPI_Type_free(&link->remote[i]);CHKERRQ(ierr);
    }
    ierr = PetscFree2(link->mine,link->remote);CHKERRQ(ierr);
    ierr = PetscFree(link);CHKERRQ(ierr);
  }
  w->link = PETSC_NULL;
  for (wlink=w->wins; wlink; wlink=wnext) {
    wnext = wlink->next;
    if (wlink->inuse) SETERRQ1(((PetscObject)sf)->comm,PETSC_ERR_ARG_WRONGSTATE,"Window still in use with address %p",(void*)wlink->addr);
    ierr = MPI_Win_free(&wlink->win);CHKERRQ(ierr);
    ierr = PetscFree(wlink);CHKERRQ(ierr);
  }
  w->wins = PETSC_NULL;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFDestroy_Window"
static PetscErrorCode PetscSFDestroy_Window(PetscSF sf)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscSFReset_Window(sf);CHKERRQ(ierr);
  ierr = PetscFree(sf->data);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)sf,"PetscSFWindowSetSyncType_C",0,0);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)sf,"PetscSFWindowGetSyncType_C",0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFView_Window"
static PetscErrorCode PetscSFView_Window(PetscSF sf,PetscViewer viewer)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;
  PetscErrorCode ierr;
  PetscBool      iascii;

  PetscFunctionBegin;
  ierr = PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&iascii);CHKERRQ(ierr);
  if (iascii) {
    ierr = PetscViewerASCIIPrintf(viewer,"  synchronization=%s sort=%s\n",PetscSFWindowSyncTypes[w->sync],sf->rankorder?"rank-order":"unordered");CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFDuplicate_Window"
static PetscErrorCode PetscSFDuplicate_Window(PetscSF sf,PetscSFDuplicateOption opt,PetscSF newsf)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscSFWindowSetSyncType(newsf,w->sync);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFBcastBegin_Window"
static PetscErrorCode PetscSFBcastBegin_Window(PetscSF sf,MPI_Datatype unit,const void *rootdata,void *leafdata)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;
  PetscErrorCode     ierr;
  PetscInt           i,nranks;
  const PetscMPIInt  *ranks;
  const MPI_Datatype *mine,*remote;
  MPI_Win            win;

  PetscFunctionBegin;
  ierr = PetscSFGetRanks(sf,&nranks,&ranks,PETSC_NULL,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscSFWindowGetDataTypes(sf,unit,&mine,&remote);CHKERRQ(ierr);
  ierr = PetscSFGetWindow(sf,unit,(void*)rootdata,PETSC_TRUE,MPI_MODE_NOPUT|MPI_MODE_NOPRECEDE,MPI_MODE_NOPUT,0,&win);CHKERRQ(ierr);
  for (i=0; i<nranks; i++) {
    if (w->sync == PETSCSF_WINDOW_SYNC_LOCK) {ierr = MPI_Win_lock(MPI_LOCK_SHARED,ranks[i],MPI_MODE_NOCHECK,win);CHKERRQ(ierr);}
    ierr = MPI_Get(leafdata,1,mine[i],ranks[i],0,1,remote[i],win);CHKERRQ(ierr);
    if (w->sync == PETSCSF_WINDOW_SYNC_LOCK) {ierr = MPI_Win_unlock(ranks[i],win);CHKERRQ(ierr);}
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFBcastEnd_Window"
PetscErrorCode PetscSFBcastEnd_Window(PetscSF sf,MPI_Datatype unit,const void *rootdata,void *leafdata)
{
  PetscErrorCode ierr;
  MPI_Win win;

  PetscFunctionBegin;
  ierr = PetscSFFindWindow(sf,unit,rootdata,&win);CHKERRQ(ierr);
  ierr = PetscSFRestoreWindow(sf,unit,rootdata,PETSC_TRUE,MPI_MODE_NOSTORE|MPI_MODE_NOSUCCEED,&win);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFReduceBegin_Window"
PetscErrorCode PetscSFReduceBegin_Window(PetscSF sf,MPI_Datatype unit,const void *leafdata,void *rootdata,MPI_Op op)
{
  PetscSF_Window     *w = (PetscSF_Window*)sf->data;
  PetscErrorCode     ierr;
  PetscInt           i,nranks;
  const PetscMPIInt  *ranks;
  const MPI_Datatype *mine,*remote;
  MPI_Win            win;

  PetscFunctionBegin;
  ierr = PetscSFGetRanks(sf,&nranks,&ranks,PETSC_NULL,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscSFWindowGetDataTypes(sf,unit,&mine,&remote);CHKERRQ(ierr);
  ierr = PetscSFWindowOpTranslate(&op);CHKERRQ(ierr);
  ierr = PetscSFGetWindow(sf,unit,rootdata,PETSC_TRUE,MPI_MODE_NOPRECEDE,0,0,&win);CHKERRQ(ierr);
  for (i=0; i<nranks; i++) {
    if (w->sync == PETSCSF_WINDOW_SYNC_LOCK) {ierr = MPI_Win_lock(MPI_LOCK_SHARED,ranks[i],MPI_MODE_NOCHECK,win);CHKERRQ(ierr);}
    ierr = MPI_Accumulate((void*)leafdata,1,mine[i],ranks[i],0,1,remote[i],op,win);CHKERRQ(ierr);
    if (w->sync == PETSCSF_WINDOW_SYNC_LOCK) {ierr = MPI_Win_unlock(ranks[i],win);CHKERRQ(ierr);}
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFReduceEnd_Window"
static PetscErrorCode PetscSFReduceEnd_Window(PetscSF sf,MPI_Datatype unit,const void *leafdata,void *rootdata,MPI_Op op)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;
  PetscErrorCode ierr;
  MPI_Win win;

  PetscFunctionBegin;
  if (!w->wins) PetscFunctionReturn(0);
  ierr = PetscSFFindWindow(sf,unit,rootdata,&win);CHKERRQ(ierr);
  ierr = MPI_Win_fence(MPI_MODE_NOSUCCEED,win);CHKERRQ(ierr);
  ierr = PetscSFRestoreWindow(sf,unit,rootdata,PETSC_TRUE,MPI_MODE_NOSUCCEED,&win);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
#undef __FUNCT__
#define __FUNCT__ "PetscSFFetchAndOpBegin_Window"
static PetscErrorCode PetscSFFetchAndOpBegin_Window(PetscSF sf,MPI_Datatype unit,void *rootdata,const void *leafdata,void *leafupdate,MPI_Op op)
{
  PetscErrorCode ierr;
  PetscInt           i,nranks;
  const PetscMPIInt  *ranks;
  const MPI_Datatype *mine,*remote;
  MPI_Win            win;

  PetscFunctionBegin;
  ierr = PetscSFGetRanks(sf,&nranks,&ranks,PETSC_NULL,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscSFWindowGetDataTypes(sf,unit,&mine,&remote);CHKERRQ(ierr);
  ierr = PetscSFWindowOpTranslate(&op);CHKERRQ(ierr);
  ierr = PetscSFGetWindow(sf,unit,rootdata,PETSC_FALSE,0,0,0,&win);CHKERRQ(ierr);
  for (i=0; i<sf->nranks; i++) {
    ierr = MPI_Win_lock(MPI_LOCK_EXCLUSIVE,sf->ranks[i],0,win);CHKERRQ(ierr);
    ierr = MPI_Get(leafupdate,1,mine[i],ranks[i],0,1,remote[i],win);CHKERRQ(ierr);
    ierr = MPI_Accumulate((void*)leafdata,1,mine[i],ranks[i],0,1,remote[i],op,win);CHKERRQ(ierr);
    ierr = MPI_Win_unlock(ranks[i],win);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFFetchAndOpEnd_Window"
static PetscErrorCode PetscSFFetchAndOpEnd_Window(PetscSF sf,MPI_Datatype unit,void *rootdata,const void *leafdata,void *leafupdate,MPI_Op op)
{
  PetscErrorCode ierr;
  MPI_Win        win;

  PetscFunctionBegin;
  ierr = PetscSFFindWindow(sf,unit,rootdata,&win);CHKERRQ(ierr);
  /* Nothing to do currently because MPI_LOCK_EXCLUSIVE is used in PetscSFFetchAndOpBegin(), rendering this implementation synchronous. */
  ierr = PetscSFRestoreWindow(sf,unit,rootdata,PETSC_FALSE,0,&win);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscSFCreate_Window"
PETSC_EXTERN_C PetscErrorCode PetscSFCreate_Window(PetscSF sf)
{
  PetscSF_Window *w = (PetscSF_Window*)sf->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  sf->ops->SetFromOptions  = PetscSFSetFromOptions_Window;
  sf->ops->Reset           = PetscSFReset_Window;
  sf->ops->Destroy         = PetscSFDestroy_Window;
  sf->ops->View            = PetscSFView_Window;
  sf->ops->Duplicate       = PetscSFDuplicate_Window;
  sf->ops->BcastBegin      = PetscSFBcastBegin_Window;
  sf->ops->BcastEnd        = PetscSFBcastEnd_Window;
  sf->ops->ReduceBegin     = PetscSFReduceBegin_Window;
  sf->ops->ReduceEnd       = PetscSFReduceEnd_Window;
  sf->ops->FetchAndOpBegin = PetscSFFetchAndOpBegin_Window;
  sf->ops->FetchAndOpEnd   = PetscSFFetchAndOpEnd_Window;

  ierr = PetscNewLog(sf,PetscSF_Window,&w);CHKERRQ(ierr);
  sf->data = (void*)w;
  w->sync = PETSCSF_WINDOW_SYNC_FENCE;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)sf,"PetscSFWindowSetSyncType_C","PetscSFWindowSetSyncType_Window",PetscSFWindowSetSyncType_Window);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)sf,"PetscSFWindowGetSyncType_C","PetscSFWindowGetSyncType_Window",PetscSFWindowGetSyncType_Window);CHKERRQ(ierr);

#if defined(OMPI_MAJOR_VERSION) && (OMPI_MAJOR_VERSION < 1 || (OMPI_MAJOR_VERSION == 1 && OMPI_MINOR_VERSION <= 6))
  {
    PetscBool ackbug = PETSC_FALSE;
    ierr = PetscOptionsGetBool(PETSC_NULL,"-acknowledge_ompi_onesided_bug",&ackbug,PETSC_NULL);CHKERRQ(ierr);
    if (ackbug) {
      ierr = PetscInfo(sf,"Acknowledged Open MPI bug, proceeding anyway. Expect memory corruption.");CHKERRQ(ierr);
    } else SETERRQ(((PetscObject)sf)->comm,PETSC_ERR_LIB,"Open MPI is known to be buggy (https://svn.open-mpi.org/trac/ompi/ticket/1905 and 2656), use -acknowledge_ompi_onesided_bug to proceed");
  }
#endif
  PetscFunctionReturn(0);
}
