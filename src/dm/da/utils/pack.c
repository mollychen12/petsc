/*$Id: pack.c,v 1.10 2000/06/20 16:18:48 bsmith Exp bsmith $*/
 
#include "petscda.h"     /*I      "petscda.h"     I*/
#include "petscmat.h"    /*I      "petscmat.h"    I*/

/*
   rstart is where an array/subvector starts in the global parallel vector, so arrays
   rstarts are meaningless (and set to the previous one) except on processor 0
*/

typedef enum {VECPACK_ARRAY, VECPACK_DA, VECPACK_VECSCATTER} VecPackLinkType;

struct VecPackLink {
  Vec                globalholder;
  DA                 da;
  int                n,rstart;      /* rstart is relative to this processor */
  VecPackLinkType    type;
  struct VecPackLink *next;
};

struct _p_VecPack {
  MPI_Comm           comm;
  int                rank;
  int                n,N,rstart;   /* rstart is relative to all processors */
  Vec                globalvector;
  int                nDA,nredundant;
  struct VecPackLink *next;
};

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackCreate"></a>*/"VecPackCreate"
/*@C
    VecPackCreate - Creates a vector packer, used to generate "composite"
      vectors made up of several subvectors.

    Collective on MPI_Comm

    Input Parameter:
.   comm - the processors that will share the global vector

    Output Parameters:
.   packer - the packer object

    Level: advanced

.seealso VecPackDestroy(), VecPackAddArray(), VecPackAddDA(), VecPackScatter(),
         VecPackGather(), VecPackCreateGlobalVector(), VecPackGetGlobalIndices()

@*/
int VecPackCreate(MPI_Comm comm,VecPack *packer)
{
  int     ierr;
  VecPack p;

  PetscFunctionBegin;
  p               = PetscNew(struct _p_VecPack);CHKPTRQ(p);
  p->n            = 0;
  p->next         = PETSC_NULL;
  p->comm         = comm;
  p->globalvector = PETSC_NULL;
  p->nredundant   = 0;
  p->nDA          = 0;
  ierr            = MPI_Comm_rank(comm,&p->rank);CHKERRQ(ierr);
  *packer = p;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackDestroy"></a>*/"VecPackDestroy"
/*@C
    VecPackDestroy - Destroys a vector packer.

    Collective on VecPack

    Input Parameter:
.   packer - the packer object

    Level: advanced

.seealso VecPackCreate(), VecPackAddArray(), VecPackAddDA(), VecPackScatter(),
         VecPackGather(), VecPackCreateGlobalVector(), VecPackGetGlobalIndices()

@*/
int VecPackDestroy(VecPack packer)
{
  int                ierr;
  struct VecPackLink *next = packer->next,*prev;

  PetscFunctionBegin;
  while (next) {
    prev = next;
    next = next->next;
    if (prev->type == VECPACK_DA) {
      ierr = DADestroy(prev->da);CHKERRQ(ierr);
      ierr = VecDestroy(prev->globalholder);CHKERRQ(ierr);
    }
    ierr = PetscFree(prev);CHKERRQ(ierr);
  }
  if (packer->globalvector) {
    ierr = VecDestroy(packer->globalvector);CHKERRQ(ierr);
  }
  ierr = PetscFree(packer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackScatter_Array"></a>*/"VecPackScatter_Array"
int VecPackScatter_Array(VecPack packer,struct VecPackLink *mine,Vec vec,Scalar *array)
{
  int    ierr;
  Scalar *varray;

  PetscFunctionBegin;

  if (!packer->rank) {
    ierr    = VecGetArray(vec,&varray);CHKERRQ(ierr);
    varray += mine->rstart;
    ierr    = PetscMemcpy(array,varray,mine->n*sizeof(Scalar));CHKERRQ(ierr);
    varray -= mine->rstart;
    ierr    = VecRestoreArray(vec,&varray);CHKERRQ(ierr);
  }
  ierr    = MPI_Bcast(array,mine->n,MPIU_SCALAR,0,packer->comm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackScatter_DA"></a>*/"VecPackScatter_DA"
int VecPackScatter_DA(VecPack packer,struct VecPackLink *mine,Vec vec,Vec local)
{
  int    ierr;
  Scalar *array;

  PetscFunctionBegin;

  ierr = VecGetArray(vec,&array);CHKERRQ(ierr);
  array += mine->rstart;
  ierr = VecPlaceArray(mine->globalholder,array);CHKERRQ(ierr);
  ierr = DAGlobalToLocalBegin(mine->da,mine->globalholder,INSERT_VALUES,local);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(mine->da,mine->globalholder,INSERT_VALUES,local);CHKERRQ(ierr);
  array -= mine->rstart;
  ierr = VecRestoreArray(vec,&array);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackGather_Array"></a>*/"VecPackGather_Array"
int VecPackGather_Array(VecPack packer,struct VecPackLink *mine,Vec vec,Scalar *array)
{
  int    ierr;
  Scalar *varray;

  PetscFunctionBegin;
  if (!packer->rank) {
    ierr    = VecGetArray(vec,&varray);CHKERRQ(ierr);
    varray += mine->rstart;
    ierr    = PetscMemcpy(varray,array,mine->n*sizeof(Scalar));CHKERRQ(ierr);
    varray -= mine->rstart;
    ierr    = VecRestoreArray(vec,&varray);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackGather_DA"></a>*/"VecPackGather_DA"
int VecPackGather_DA(VecPack packer,struct VecPackLink *mine,Vec vec,Vec local)
{
  int    ierr;
  Scalar *array;

  PetscFunctionBegin;

  ierr = VecGetArray(vec,&array);CHKERRQ(ierr);
  array += mine->rstart;
  ierr = VecPlaceArray(mine->globalholder,array);CHKERRQ(ierr);
  ierr = DALocalToGlobal(mine->da,local,INSERT_VALUES,mine->globalholder);CHKERRQ(ierr);
  array -= mine->rstart;
  ierr = VecRestoreArray(vec,&array);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#include <stdarg.h>

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackScatter"></a>*/"VecPackScatter"
/*@C
    VecPackScatter - Scatters from a global packed vector into its individual local vectors

    Collective on VecPack

    Input Parameter:
+    packer - the packer object
.    gvec - the global vector
-    ... - the individual sequential objects (arrays or vectors)
 
    Level: advanced

.seealso VecPackDestroy(), VecPackAddArray(), VecPackAddDA(), VecPackCreateGlobalVector(),
         VecPackGather(), VecPackCreate(), VecPackGetGlobalIndices()

@*/
int VecPackScatter(VecPack packer,Vec gvec,...)
{
  va_list            Argp;
  int                ierr;
  struct VecPackLink *next = packer->next;

  PetscFunctionBegin;
  if (!packer->globalvector) {
    SETERRQ(1,1,"Must first create global vector with VecPackCreateGlobalVector()");
  }

  /* loop over packed objects, handling one at at time */
  va_start(Argp,gvec);
  while (next) {
    if (next->type == VECPACK_ARRAY) {
      Scalar *array;
      array = va_arg(Argp, Scalar*);
      ierr = VecPackScatter_Array(packer,next,gvec,array);CHKERRQ(ierr);
    } else if (next->type == VECPACK_DA) {
      Vec vec;
      vec = va_arg(Argp, Vec);
      PetscValidHeaderSpecific(vec,VEC_COOKIE);
      ierr = VecPackScatter_DA(packer,next,gvec,vec);CHKERRQ(ierr);
    } else {
      SETERRQ(1,1,"Cannot handle that object type yet");
    }
    next = next->next;
  }
  va_end(Argp);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackGather"></a>*/"VecPackGather"
/*@C
    VecPackGather - Gathers into a global packed vector from its individual local vectors

    Collective on VecPack

    Input Parameter:
+    packer - the packer object
.    gvec - the global vector
-    ... - the individual sequential objects (arrays or vectors)
 
    Level: advanced

.seealso VecPackDestroy(), VecPackAddArray(), VecPackAddDA(), VecPackCreateGlobalVector(),
         VecPackScatter(), VecPackCreate(), VecPackGetGlobalIndices()

@*/
int VecPackGather(VecPack packer,Vec gvec,...)
{
  va_list            Argp;
  int                ierr;
  struct VecPackLink *next = packer->next;

  PetscFunctionBegin;
  if (!packer->globalvector) {
    SETERRQ(1,1,"Must first create global vector with VecPackCreateGlobalVector()");
  }

  /* loop over packed objects, handling one at at time */
  va_start(Argp,gvec);
  while (next) {
    if (next->type == VECPACK_ARRAY) {
      Scalar *array;
      array = va_arg(Argp, Scalar*);
      ierr  = VecPackGather_Array(packer,next,gvec,array);CHKERRQ(ierr);
    } else if (next->type == VECPACK_DA) {
      Vec vec;
      vec = va_arg(Argp, Vec);
      PetscValidHeaderSpecific(vec,VEC_COOKIE);
      ierr = VecPackGather_DA(packer,next,gvec,vec);CHKERRQ(ierr);
    } else {
      SETERRQ(1,1,"Cannot handle that object type yet");
    }
    next = next->next;
  }
  va_end(Argp);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackAddArray"></a>*/"VecPackAddArray"
/*@C
    VecPackAddArray - adds an "redundant" array to a VecPack. The array values will 
       be stored in part of the array on processor 0.

    Collective on VecPack

    Input Parameter:
+    packer - the packer object
-    n - the length of the array
 
    Level: advanced

.seealso VecPackDestroy(), VecPackGather(), VecPackAddDA(), VecPackCreateGlobalVector(),
         VecPackScatter(), VecPackCreate(), VecPackGetGlobalIndices()

@*/
int VecPackAddArray(VecPack packer,int n)
{
  struct VecPackLink *mine,*next = packer->next;

  PetscFunctionBegin;
  if (packer->globalvector) {
    SETERRQ(1,1,"Cannot add an array once you have called VecPackCreateGlobalVector()");
  }

  /* create new link */
  mine               = PetscNew(struct VecPackLink);CHKPTRQ(mine);
  mine->n            = n;
  mine->da           = PETSC_NULL;
  mine->globalholder = PETSC_NULL;
  mine->type         = VECPACK_ARRAY;
  if (!packer->rank) packer->n += n;

  /* add to end of list */
  if (!next) {
    packer->next = mine;
  } else {
    while (next->next) next = next->next;
    next->next = mine;
  }
  packer->nredundant++;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackAddDA"></a>*/"VecPackAddDA"
/*@C
    VecPackAddDA - adds a DA vector to a VecPack

    Collective on VecPack

    Input Parameter:
+    packer - the packer object
-    da - the DA object
 
    Level: advanced

.seealso VecPackDestroy(), VecPackGather(), VecPackAddDA(), VecPackCreateGlobalVector(),
         VecPackScatter(), VecPackCreate(), VecPackGetGlobalIndices()

@*/
int VecPackAddDA(VecPack packer,DA da)
{
  int                ierr,n;
  struct VecPackLink *mine,*next = packer->next;

  PetscFunctionBegin;
  if (packer->globalvector) {
    SETERRQ(1,1,"Cannot add a DA once you have called VecPackCreateGlobalVector()");
  }

  /* create new link */
  mine               = PetscNew(struct VecPackLink);CHKPTRQ(mine);
  ierr = PetscObjectReference((PetscObject)da);CHKERRQ(ierr);
  ierr = DACreateGlobalVector(da,&mine->globalholder);CHKERRQ(ierr);
  ierr = VecGetLocalSize(mine->globalholder,&n);CHKERRQ(ierr);
  mine->n      = n;
  mine->da     = da;  
  mine->type   = VECPACK_DA;
  packer->n   += n;

  /* add to end of list */
  if (!next) {
    packer->next = mine;
  } else {
    while (next->next) next = next->next;
    next->next = mine;
  }
  packer->nDA++;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackCreateGlobalVector"></a>*/"VecPackCreateGlobalVector"
/*@C
    VecPackCreateGlobalVector - Creates a vector of the correct size to be gathered into 
        by the packer.

    Collective on VecPack

    Input Parameter:
.    packer - the packer object

    Output Parameters:
.   gvec - the global vector

    Level: advanced

    Notes: Once this has been created you cannot add additional arrays or vectors to be packed.

.seealso VecPackDestroy(), VecPackAddArray(), VecPackAddDA(), VecPackScatter(),
         VecPackGather(), VecPackCreate(), VecPackGetGlobalIndices()

@*/
int VecPackCreateGlobalVector(VecPack packer,Vec *gvec)
{
  int                ierr,nprev = 0,rank;
  struct VecPackLink *next = packer->next;

  PetscFunctionBegin;
  if (packer->globalvector) {
    ierr = VecDuplicate(packer->globalvector,gvec);CHKERRQ(ierr);
  } else {
    ierr = VecCreateMPI(packer->comm,packer->n,PETSC_DETERMINE,gvec);CHKERRQ(ierr);
    ierr = PetscObjectReference((PetscObject)*gvec);CHKERRQ(ierr);
    packer->globalvector = *gvec;

    ierr = VecGetSize(*gvec,&packer->N);CHKERRQ(ierr);
    ierr = VecGetOwnershipRange(*gvec,&packer->rstart,PETSC_NULL);CHKERRQ(ierr);
    
    /* now set the rstart for each linked array/vector */
    ierr = MPI_Comm_rank(packer->comm,&rank);CHKERRQ(ierr);
    while (next) {
      next->rstart = nprev; 
      if (!rank || next->type != VECPACK_ARRAY) nprev += next->n;
      next         = next->next;
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name="VecPackGetGlobalIndices"></a>*/"VecPackGetGlobalIndices"
/*@C
    VecPackGetGlobalIndices - Gets the global indices for all the entries in the packed
      vectors.

    Collective on VecPack

    Input Parameter:
.    packer - the packer object

    Output Parameters:
.    idx - the individual indices for each packed vector/array
 
    Level: advanced

    Notes:
       The idx parameters should be freed by the calling routine with PetscFree()

.seealso VecPackDestroy(), VecPackAddArray(), VecPackAddDA(), VecPackCreateGlobalVector(),
         VecPackGather(), VecPackCreate()

@*/
int VecPackGetGlobalIndices(VecPack packer,...)
{
  va_list            Argp;
  int                ierr,i,**idx,n;
  struct VecPackLink *next = packer->next;
  Vec                global;
  PF                 pf;
  Scalar             *array;

  PetscFunctionBegin;
  ierr = VecPackCreateGlobalVector(packer,&global);CHKERRQ(ierr);

  /* put 0 to N-1 into the global vector */
  ierr = PFCreate(PETSC_COMM_WORLD,1,1,&pf);CHKERRQ(ierr);
  ierr = PFSetType(pf,PFIDENTITY,PETSC_NULL);CHKERRQ(ierr);
  ierr = PFApplyVec(pf,PETSC_NULL,global);CHKERRQ(ierr);
  ierr = PFDestroy(pf);CHKERRQ(ierr);

  /* loop over packed objects, handling one at at time */
  va_start(Argp,packer);
  while (next) {
    idx = va_arg(Argp, int**);

    if (next->type == VECPACK_ARRAY) {
      
      *idx   = (int*)PetscMalloc(next->n*sizeof(int));CHKPTRQ(*idx);
      if (!packer->rank) {
        ierr   = VecGetArray(global,&array);CHKERRQ(ierr);
        array += next->rstart;
        for (i=0; i<next->n; i++) (*idx)[i] = (int)PetscRealPart(array[i]);
        array -= next->rstart;
        ierr   = VecRestoreArray(global,&array);CHKERRQ(ierr);
      }
      ierr = MPI_Bcast(*idx,next->n,MPI_INT,0,packer->comm);CHKERRQ(ierr);

    } else if (next->type == VECPACK_DA) {
      Vec local;

      ierr    = DACreateLocalVector(next->da,&local);CHKERRQ(ierr);
      ierr    = VecGetArray(global,&array);CHKERRQ(ierr);
      array  += next->rstart;
      ierr    = VecPlaceArray(next->globalholder,array);CHKERRQ(ierr);
      ierr    = DAGlobalToLocalBegin(next->da,next->globalholder,INSERT_VALUES,local);CHKERRQ(ierr);
      ierr    = DAGlobalToLocalEnd(next->da,next->globalholder,INSERT_VALUES,local);CHKERRQ(ierr);
      array  -= next->rstart;
      ierr    = VecRestoreArray(global,&array);CHKERRQ(ierr);

      ierr    = VecGetArray(local,&array);CHKERRQ(ierr);
      ierr    = VecGetSize(local,&n);CHKERRQ(ierr);
      *idx    = (int*)PetscMalloc(n*sizeof(int));CHKPTRQ(*idx);
      for (i=0; i<n; i++) (*idx)[i] = (int)PetscRealPart(array[i]);
      ierr    = VecRestoreArray(local,&array);CHKERRQ(ierr);
      ierr    = VecDestroy(local);CHKERRQ(ierr);

    } else {
      SETERRQ(1,1,"Cannot handle that object type yet");
    }
    next = next->next;
  }
  va_end(Argp);
  ierr = VecDestroy(global);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}




