/*$Id: matadic.c,v 1.8 2001/07/26 19:57:18 bsmith Exp bsmith $*/
/*
    ADIC matrix-free matrix implementation
*/

#include "petscsys.h"
#include "src/mat/matimpl.h"   /*I   "mat.h"  I*/
#include "petscda.h"
#include "petscsnes.h"

typedef struct {
  DA   da;
  Vec  localu; /* point at which Jacobian is evaluated */
  void *ctx;
  SNES snes;
} Mat_DAAD;

#undef __FUNCT__  
#define __FUNCT__ "MatAssemblyEnd_DAAD"
int MatAssemblyEnd_DAAD(Mat A,MatAssemblyType atype)
{
  Mat_DAAD *a = (Mat_DAAD*)A->data;
  int      ierr;
  Vec      u;

  PetscFunctionBegin;
  if (a->snes) {
    ierr = SNESGetSolution(a->snes,&u);CHKERRQ(ierr);
    ierr = DAGlobalToLocalBegin(a->da,u,INSERT_VALUES,a->localu);CHKERRQ(ierr);
    ierr = DAGlobalToLocalEnd(a->da,u,INSERT_VALUES,a->localu);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMult_DAAD"
int MatMult_DAAD(Mat A,Vec xx,Vec yy)
{
  Mat_DAAD *a = (Mat_DAAD*)A->data;
  Vec      localxx;
  int      ierr;

  PetscFunctionBegin;
  ierr = DAGetLocalVector(a->da,&localxx);CHKERRQ(ierr);
  ierr = DAGlobalToLocalBegin(a->da,xx,INSERT_VALUES,localxx);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(a->da,xx,INSERT_VALUES,localxx);CHKERRQ(ierr);
  ierr = DAMultiplyByJacobian1WithAD(a->da,a->localu,localxx,yy,a->ctx);CHKERRQ(ierr);
  ierr = DARestoreLocalVector(a->da,&localxx);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetDiagonal_DAD"
int MatGetDiagonal_DAAD(Mat A,Vec d)
{
  Mat_DAAD *a = (Mat_DAAD*)A->data;
  int      ierr,rstart,rend,i;
  Scalar   *aa;

  PetscFunctionBegin;
  ierr = VecGetOwnershipRange(d,&rstart,&rend);CHKERRQ(ierr);
  ierr = VecGetArray(d,&aa);CHKERRQ(ierr);
  for (i=rstart; i<rend; i++) {
    ierr = DAComputeJacobiani1WithAdic(a->da,i,a->localu,aa+i-rstart,a->ctx);CHKERRQ(ierr);
  }
  ierr = VecRestoreArray(d,&aa);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#include "src/dm/da/daimpl.h"

typedef struct  {
 double  u[8];
}
mField;

typedef struct  {
 double  u[4];
}
iField;

#undef __FUNCT__  
#define __FUNCT__ "MatRelax_DAD"
int MatRelax_DAAD(Mat A,Vec bb,PetscReal omega,MatSORType flag,PetscReal fshift,int its,Vec xx)
{
  Mat_DAAD    *a = (Mat_DAAD*)A->data;
  int         ierr,rstart,rend,j,gtdof,nI;
  Scalar      *aa,result,*x,*avu,*av,*ad_vustart,ad_f[2],zero = 0.0,*D;
  Vec         localxx,dd;
  DALocalInfo info;
  MatStencil  stencil;
  mField      **ad_vu;
  iField      **b,**d;

  PetscFunctionBegin;
  ierr = DAGetGlobalVector(a->da,&dd);CHKERRQ(ierr);
  ierr = MatGetDiagonal(A,dd);CHKERRQ(ierr);

  ierr = DAGetLocalVector(a->da,&localxx);CHKERRQ(ierr);
  if (flag & SOR_ZERO_INITIAL_GUESS) {
    ierr = VecSet(&zero,localxx);CHKERRQ(ierr);
  } else {
    ierr = DAGlobalToLocalBegin(a->da,xx,INSERT_VALUES,localxx);CHKERRQ(ierr);
    ierr = DAGlobalToLocalEnd(a->da,xx,INSERT_VALUES,localxx);CHKERRQ(ierr);
  }

  /* get space for derivative objects.  */
  ierr = DAGetAdicMFArray(a->da,PETSC_TRUE,(void **)&ad_vu,(void**)&ad_vustart,&gtdof);CHKERRQ(ierr);

  /* copy input vector into derivative object */
  ierr = VecGetArray(a->localu,&avu);CHKERRQ(ierr);
  ierr = VecGetArray(localxx,&av);CHKERRQ(ierr);
  for (j=0; j<gtdof; j++) {
    ad_vustart[2*j]   = avu[j];
    ad_vustart[2*j+1] = av[j];
  }
  ierr = VecRestoreArray(a->localu,&avu);CHKERRQ(ierr);
  ierr = VecRestoreArray(localxx,&av);CHKERRQ(ierr);

  my_AD_ResetIndep();
  my_AD_IncrementTotalGradSize(1);
  my_AD_SetIndepDone();

  ierr = VecGetArray(dd,&D);CHKERRQ(ierr);
  ierr = VecRestoreArray(dd,0);CHKERRQ(ierr);
  ierr = DAVecGetArray(a->da,bb,(void**)&b);CHKERRQ(ierr);
  ierr = DAVecGetArray(a->da,dd,(void**)&d);CHKERRQ(ierr);

  ierr = DAGetLocalInfo(a->da,&info);CHKERRQ(ierr);
  if (flag & SOR_FORWARD_SWEEP || flag & SOR_LOCAL_FORWARD_SWEEP){
    nI = 0;
    for (stencil.j = info.ys; stencil.j<info.ys+info.ym; stencil.j++) {
      for (stencil.i = info.xs; stencil.i<info.xs+info.xm; stencil.i++) {
        for (stencil.c = 0; stencil.c<info.dof; stencil.c++) {
          ierr = (*a->da->adicmf_lfi)(&info,&stencil,ad_vu,ad_f,a->ctx);CHKERRQ(ierr);

          ad_vu[stencil.j][stencil.i].u[2*stencil.c+1] += (b[stencil.j][stencil.i].u[stencil.c] - ad_f[1])/d[stencil.j][stencil.i].u[stencil.c];
          if (d[stencil.j][stencil.i].u[stencil.c] != D[nI]) SETERRQ(1,"");
          nI++;
        }
      }
    }
  }

  ierr = DAVecRestoreArray(a->da,bb,(void**)&b);CHKERRQ(ierr);
  ierr = DAVecRestoreArray(a->da,dd,(void**)&d);CHKERRQ(ierr);
  /*  ierr = VecRestoreArray(dd,&D);CHKERRQ(ierr); */
  ierr = DARestoreGlobalVector(a->da,&dd);CHKERRQ(ierr);

  ierr = VecGetArray(localxx,&av);CHKERRQ(ierr);
  for (j=0; j<gtdof; j++) {
    av[j] = ad_vustart[2*j+1];
  }
  ierr = VecRestoreArray(localxx,&av);CHKERRQ(ierr);
  /*  VecView(localxx,0); */

  ierr = DALocalToGlobal(a->da,localxx,INSERT_VALUES,xx);CHKERRQ(ierr);
  /*  ierr = VecCopy(bb,xx);CHKERRQ(ierr); */

  ierr = DARestoreLocalVector(a->da,&localxx);CHKERRQ(ierr);
  ierr = DARestoreAdicMFArray(a->da,PETSC_TRUE,(void **)&ad_vu,(void**)&ad_vustart,&gtdof);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}



#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_DAAD"
int MatDestroy_DAAD(Mat A)
{
  Mat_DAAD *a = (Mat_DAAD*)A->data;
  int      ierr;

  PetscFunctionBegin;
  ierr = DADestroy(a->da);CHKERRQ(ierr);
  ierr = VecDestroy(a->localu);CHKERRQ(ierr);
  ierr = PetscFree(a);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* -------------------------------------------------------------------*/
static struct _MatOps MatOps_Values = {0,
       0,
       0,
       MatMult_DAAD,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       MatRelax_DAAD,
       0,
       0,
       0,
       MatGetDiagonal_DAAD,
       0,
       0,
       0,
       MatAssemblyEnd_DAAD,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       MatDestroy_DAAD,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0};

/* --------------------------------------------------------------------------------*/

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatSNESMFSetBase_AD"
int MatSNESMFSetBase_AD(Mat J,Vec U)
{
  int      ierr;
  Mat_DAAD *a = (Mat_DAAD*)J->data;

  PetscFunctionBegin;
  ierr = DAGlobalToLocalBegin(a->da,U,INSERT_VALUES,a->localu);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(a->da,U,INSERT_VALUES,a->localu);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatDAADSetDA_AD"
int MatDAADSetDA_AD(Mat A,DA da)
{
  Mat_DAAD *a = (Mat_DAAD*)A->data;
  int      ierr,nc,nx,ny,nz,Nx,Ny,Nz;

  PetscFunctionBegin;
  a->da = da;
  ierr  = PetscObjectReference((PetscObject)da);CHKERRQ(ierr);
  ierr  = DAGetInfo(da,0,&Nx,&Ny,&Nz,0,0,0,&nc,0,0,0);CHKERRQ(ierr);
  ierr  = DAGetCorners(da,0,0,0,&nx,&ny,&nz);CHKERRQ(ierr);
  A->m  = A->n = nc*nx*ny*nz;
  A->M  = A->N = nc*Nx*Ny*Nz;
  ierr  = DACreateLocalVector(da,&a->localu);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatDAADSetSNES_AD"
int MatDAADSetSNES_AD(Mat A,SNES snes)
{
  Mat_DAAD *a = (Mat_DAAD*)A->data;

  PetscFunctionBegin;
  a->snes = snes;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatDAADSetCtx_AD"
int MatDAADSetCtx_AD(Mat A,void *ctx)
{
  Mat_DAAD *a = (Mat_DAAD*)A->data;

  PetscFunctionBegin;
  a->ctx = ctx;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatCreate_DAAD"
int MatCreate_DAAD(Mat B)
{
  Mat_DAAD *b;
  int      ierr;

  PetscFunctionBegin;
  ierr    = PetscNew(Mat_DAAD,&b);CHKERRQ(ierr);
  B->data = (void*)b;
  ierr = PetscMemzero(b,sizeof(Mat_DAAD));CHKERRQ(ierr);
  ierr = PetscMemcpy(B->ops,&MatOps_Values,sizeof(struct _MatOps));CHKERRQ(ierr);
  
  ierr = PetscMapCreateMPI(B->comm,B->m,B->m,&B->rmap);CHKERRQ(ierr);
  ierr = PetscMapCreateMPI(B->comm,B->n,B->n,&B->cmap);CHKERRQ(ierr);

  ierr = PetscObjectChangeTypeName((PetscObject)B,MATDAAD);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatSNESMFSetBase_C","MatSNESMFSetBase_AD",MatSNESMFSetBase_AD);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatDAADSetDA_C","MatDAADSetDA_AD",MatDAADSetDA_AD);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatDAADSetSNES_C","MatDAADSetSNES_AD",MatDAADSetSNES_AD);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)B,"MatDAADSetCtx_C","MatDAADSetCtx_AD",MatDAADSetCtx_AD);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END


#undef __FUNCT__  
#define __FUNCT__ "MatDAADSetDA"
/*@C
   MatDAADSetDA - Tells the matrix what DA it is using for layout and Jacobian.

   Collective on Mat and DA

   Input Parameters:
+  mat - the matrix
-  da - the DA

   Level: intermediate

.seealso: MatCreate(), DASetLocalAdicMFFunction(), MatCreateDAAD()

@*/
int MatDAADSetDA(Mat A,DA da)
{
  int      ierr,(*f)(Mat,void*);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A,MAT_COOKIE);
  PetscValidHeader(da);
  ierr = PetscObjectQueryFunction((PetscObject)A,"MatDAADSetDA_C",(void (**)())&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(A,da);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDAADSetSNES"
/*@C
   MatDAADSetSNES - Tells the matrix what SNES it is using for the base U.

   Collective on Mat and SNES

   Input Parameters:
+  mat - the matrix
-  snes - the SNES

   Level: intermediate

.seealso: MatCreate(), DASetLocalAdicMFFunction(), MatCreateDAAD(), MatDAADSetDA()

@*/
int MatDAADSetSNES(Mat A,SNES snes)
{
  int      ierr,(*f)(Mat,void*);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A,MAT_COOKIE);
  PetscValidHeader(snes);
  ierr = PetscObjectQueryFunction((PetscObject)A,"MatDAADSetSNES_C",(void (**)())&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(A,snes);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDAADSetCtx"
/*@C
   MatDAADSetCtx - Sets the user context for a DAAD (ADIC matrix-free) matrix.

   Collective on Mat

   Input Parameters:
+  mat - the matrix
-  ctx - the context

   Level: intermediate

.seealso: MatCreate(), DASetLocalAdicMFFunction(), MatCreateDAAD(), MatDAADSetDA()

@*/
int MatDAADSetCtx(Mat A,void *ctx)
{
  int      ierr,(*f)(Mat,void*);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A,MAT_COOKIE);
  ierr = PetscObjectQueryFunction((PetscObject)A,"MatDAADSetCtx_C",(void (**)())&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(A,ctx);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatCreateDAAD"
/*@C
   MatCreateDAAD - Creates a matrix that can do matrix-vector products using a local 
   function that is differentiated with ADIFOR or ADIC.

   Collective on DA

   Input Parameters:
.  da - the DA that defines the distribution of the vectors

   Output Parameter:
.  A - the matrix 

   Level: intermediate

.seealso: MatCreate(), DASetLocalAdicMFFunction()

@*/
int MatCreateDAAD(DA da,Mat *A)
{
  int      ierr;
  MPI_Comm comm;

  PetscFunctionBegin;
  ierr = PetscObjectGetComm((PetscObject)da,&comm);CHKERRQ(ierr);
  ierr = MatCreate(comm,0,0,0,0,A);CHKERRQ(ierr);
  ierr = MatSetType(*A,MATDAAD);CHKERRQ(ierr);
  ierr = MatDAADSetDA(*A,da);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatRegisterDAAD"
int MatRegisterDAAD(void)
{ 
  int ierr;
  PetscFunctionBegin;
  ierr = MatRegisterDynamic(MATDAAD,PETSC_NULL,"MatCreate_DAAD",MatCreate_DAAD);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
