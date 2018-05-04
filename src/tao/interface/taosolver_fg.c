#include <petsc/private/taoimpl.h> /*I "petsctao.h" I*/

/*@
  TaoSetInitialVector - Sets the initial guess for the solve

  Logically collective on Tao

  Input Parameters:
+ tao - the Tao context
- x0  - the initial guess

  Level: beginner
.seealso: TaoCreate(), TaoSolve()
@*/

PetscErrorCode TaoSetInitialVector(Tao tao, Vec x0)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  if (x0) {
    PetscValidHeaderSpecific(x0,VEC_CLASSID,2);
    PetscObjectReference((PetscObject)x0);
  }
  ierr = VecDestroy(&tao->solution);CHKERRQ(ierr);
  tao->solution = x0;
  PetscFunctionReturn(0);
}

PetscErrorCode TaoTestGradient(Tao tao,Vec g1)
{
  Vec              x = tao->solution,g2,g3;
  PetscBool        complete_print = PETSC_FALSE,test = PETSC_FALSE;
  PetscReal        hcnorm,fdnorm,hcmax,fdmax,diffmax,diffnorm;
  PetscScalar      dot;
  MPI_Comm         comm;
  PetscViewer      viewer;
  PetscInt         tabs;
  static PetscBool directionsprinted = PETSC_FALSE;
  PetscErrorCode   ierr;

  PetscFunctionBegin;
  ierr = PetscObjectOptionsBegin((PetscObject)tao);CHKERRQ(ierr);
  ierr = PetscOptionsName("-tao_test_gradient","Compare hand-coded and finite difference Gradients","None",&test);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-tao_test_gradient_view","View difference between hand-coded and finite difference Gradients element entries","None",complete_print,&complete_print,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  if (!test) PetscFunctionReturn(0);

  ierr = PetscObjectGetComm((PetscObject)tao,&comm);CHKERRQ(ierr);
  ierr = PetscViewerASCIIGetStdout(comm,&viewer);CHKERRQ(ierr);
  ierr = PetscViewerASCIIGetTab(viewer, &tabs);CHKERRQ(ierr);
  ierr = PetscViewerASCIISetTab(viewer, ((PetscObject)tao)->tablevel);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ---------- Testing Gradient -------------\n");CHKERRQ(ierr);
  if (!complete_print && !directionsprinted) {
    ierr = PetscViewerASCIIPrintf(viewer,"  Run with -tao_test_gradient_view and optionally -tao_test_gradient <threshold> to show difference\n");CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"    of hand-coded and finite difference gradient entries greater than <threshold>.\n");CHKERRQ(ierr);
  }
  if (!directionsprinted) {
    ierr = PetscViewerASCIIPrintf(viewer,"  Testing hand-coded Gradient, if (for double precision runs) ||G - Gfd||_F/||G||_F is\n");CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"    O(1.e-8), the hand-coded Hessian is probably correct.\n");CHKERRQ(ierr);
    directionsprinted = PETSC_TRUE;
  }

  ierr = VecDuplicate(x,&g2);CHKERRQ(ierr);
  ierr = VecDuplicate(x,&g3);CHKERRQ(ierr);

  /* Compute finite difference gradient, assume the gradient is already computed by TaoComputeGradient() and put into g1 */
  ierr = TaoDefaultComputeGradient(tao,x,g2,NULL);CHKERRQ(ierr);

  ierr = VecNorm(g2,NORM_2,&fdnorm);CHKERRQ(ierr);
  ierr = VecNorm(g1,NORM_2,&hcnorm);CHKERRQ(ierr);
  ierr = VecNorm(g2,NORM_INFINITY,&fdmax);CHKERRQ(ierr);
  ierr = VecNorm(g1,NORM_INFINITY,&hcmax);CHKERRQ(ierr);
  ierr = VecDot(g1,g2,&dot);CHKERRQ(ierr);
  ierr = VecCopy(g1,g3);CHKERRQ(ierr);
  ierr = VecAXPY(g3,-1.0,g2);CHKERRQ(ierr);
  ierr = VecNorm(g3,NORM_2,&diffnorm);CHKERRQ(ierr);
  ierr = VecNorm(g3,NORM_INFINITY,&diffmax);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  ||Gfd|| %g, ||G|| = %g, angle cosine = (Gfd'G)/||Gfd||||G|| = %g\n", (double)fdnorm, (double)hcnorm, (double)(PetscRealPart(dot)/(fdnorm*hcnorm)));CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  2-norm ||G - Gfd||/||G|| = %g, ||G - Gfd|| = %g\n",(double)(diffnorm/PetscMax(hcnorm,fdnorm)),(double)diffnorm);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  max-norm ||G - Gfd||/||G|| = %g, ||G - Gfd|| = %g\n",(double)(diffmax/PetscMax(hcmax,fdmax)),(double)diffmax);CHKERRQ(ierr);

  if (complete_print) {
    ierr = PetscViewerASCIIPrintf(viewer,"  Hand-coded gradient ----------\n");CHKERRQ(ierr);
    ierr = VecView(g1,viewer);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"  Finite difference gradient ----------\n");CHKERRQ(ierr);
    ierr = VecView(g2,viewer);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"  Hand-coded minus finite-difference gradient ----------\n");CHKERRQ(ierr);
    ierr = VecView(g3,viewer);CHKERRQ(ierr);
  }
  ierr = VecDestroy(&g2);CHKERRQ(ierr);
  ierr = VecDestroy(&g3);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  TaoComputeGradient - Computes the gradient of the objective function

  Collective on Tao

  Input Parameters:
+ tao - the Tao context
- X - input vector

  Output Parameter:
. G - gradient vector

  Options Database Keys:
+    -tao_test_gradient - compare the user provided gradient with one compute via finite differences to check for errors
-    -tao_test_gradient_display - display the user provided gradient, the finite difference gradient and the difference between them to help users detect the location of errors in the user provided gradient

  Notes:
    TaoComputeGradient() is typically used within minimization implementations,
  so most users would not generally call this routine themselves.

  Level: advanced

.seealso: TaoComputeObjective(), TaoComputeObjectiveAndGradient(), TaoSetGradientRoutine()
@*/
PetscErrorCode TaoComputeGradient(Tao tao, Vec X, Vec G)
{
  PetscErrorCode ierr;
  PetscReal      dummy;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  PetscValidHeaderSpecific(X,VEC_CLASSID,2);
  PetscValidHeaderSpecific(G,VEC_CLASSID,2);
  PetscCheckSameComm(tao,1,X,2);
  PetscCheckSameComm(tao,1,G,3);
  ierr = VecLockPush(X);CHKERRQ(ierr);
  if (tao->ops->computegradient) {
    ierr = PetscLogEventBegin(Tao_GradientEval,tao,X,G,NULL);CHKERRQ(ierr);
    PetscStackPush("Tao user gradient evaluation routine");
    ierr = (*tao->ops->computegradient)(tao,X,G,tao->user_gradP);CHKERRQ(ierr);
    PetscStackPop;
    ierr = PetscLogEventEnd(Tao_GradientEval,tao,X,G,NULL);CHKERRQ(ierr);
    tao->ngrads++;
  } else if (tao->ops->computeobjectiveandgradient) {
    ierr = PetscLogEventBegin(Tao_ObjGradientEval,tao,X,G,NULL);CHKERRQ(ierr);
    PetscStackPush("Tao user objective/gradient evaluation routine");
    ierr = (*tao->ops->computeobjectiveandgradient)(tao,X,&dummy,G,tao->user_objgradP);CHKERRQ(ierr);
    PetscStackPop;
    ierr = PetscLogEventEnd(Tao_ObjGradientEval,tao,X,G,NULL);CHKERRQ(ierr);
    tao->nfuncgrads++;
  } else SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONGSTATE,"TaoSetGradientRoutine() has not been called");
  ierr = VecLockPop(X);CHKERRQ(ierr);

  ierr = TaoTestGradient(tao,G);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  TaoComputeObjective - Computes the objective function value at a given point

  Collective on Tao

  Input Parameters:
+ tao - the Tao context
- X - input vector

  Output Parameter:
. f - Objective value at X

  Notes:
    TaoComputeObjective() is typically used within minimization implementations,
  so most users would not generally call this routine themselves.

  Level: advanced

.seealso: TaoComputeGradient(), TaoComputeObjectiveAndGradient(), TaoSetObjectiveRoutine()
@*/
PetscErrorCode TaoComputeObjective(Tao tao, Vec X, PetscReal *f)
{
  PetscErrorCode ierr;
  Vec            temp;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  PetscValidHeaderSpecific(X,VEC_CLASSID,2);
  PetscCheckSameComm(tao,1,X,2);
  ierr = VecLockPush(X);CHKERRQ(ierr);
  if (tao->ops->computeobjective) {
    ierr = PetscLogEventBegin(Tao_ObjectiveEval,tao,X,NULL,NULL);CHKERRQ(ierr);
    PetscStackPush("Tao user objective evaluation routine");
    ierr = (*tao->ops->computeobjective)(tao,X,f,tao->user_objP);CHKERRQ(ierr);
    PetscStackPop;
    ierr = PetscLogEventEnd(Tao_ObjectiveEval,tao,X,NULL,NULL);CHKERRQ(ierr);
    tao->nfuncs++;
  } else if (tao->ops->computeobjectiveandgradient) {
    ierr = PetscInfo(tao,"Duplicating variable vector in order to call func/grad routine\n");CHKERRQ(ierr);
    ierr = VecDuplicate(X,&temp);CHKERRQ(ierr);
    ierr = PetscLogEventBegin(Tao_ObjGradientEval,tao,X,NULL,NULL);CHKERRQ(ierr);
    PetscStackPush("Tao user objective/gradient evaluation routine");
    ierr = (*tao->ops->computeobjectiveandgradient)(tao,X,f,temp,tao->user_objgradP);CHKERRQ(ierr);
    PetscStackPop;
    ierr = PetscLogEventEnd(Tao_ObjGradientEval,tao,X,NULL,NULL);CHKERRQ(ierr);
    ierr = VecDestroy(&temp);CHKERRQ(ierr);
    tao->nfuncgrads++;
  }  else SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONGSTATE,"TaoSetObjectiveRoutine() has not been called");
  ierr = PetscInfo1(tao,"TAO Function evaluation: %20.19e\n",(double)(*f));CHKERRQ(ierr);
  ierr = VecLockPop(X);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  TaoComputeObjectiveAndGradient - Computes the objective function value at a given point

  Collective on Tao

  Input Parameters:
+ tao - the Tao context
- X - input vector

  Output Parameter:
+ f - Objective value at X
- g - Gradient vector at X

  Notes:
    TaoComputeObjectiveAndGradient() is typically used within minimization implementations,
  so most users would not generally call this routine themselves.

  Level: advanced

.seealso: TaoComputeGradient(), TaoComputeObjectiveAndGradient(), TaoSetObjectiveRoutine()
@*/
PetscErrorCode TaoComputeObjectiveAndGradient(Tao tao, Vec X, PetscReal *f, Vec G)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  PetscValidHeaderSpecific(X,VEC_CLASSID,2);
  PetscValidHeaderSpecific(G,VEC_CLASSID,4);
  PetscCheckSameComm(tao,1,X,2);
  PetscCheckSameComm(tao,1,G,4);
  ierr = VecLockPush(X);CHKERRQ(ierr);
  if (tao->ops->computeobjectiveandgradient) {
    ierr = PetscLogEventBegin(Tao_ObjGradientEval,tao,X,G,NULL);CHKERRQ(ierr);
    if (tao->ops->computegradient == TaoDefaultComputeGradient) {
      ierr = TaoComputeObjective(tao,X,f);CHKERRQ(ierr);
      ierr = TaoDefaultComputeGradient(tao,X,G,NULL);CHKERRQ(ierr);
    } else {
      PetscStackPush("Tao user objective/gradient evaluation routine");
      ierr = (*tao->ops->computeobjectiveandgradient)(tao,X,f,G,tao->user_objgradP);CHKERRQ(ierr);
      PetscStackPop;
    }
    ierr = PetscLogEventEnd(Tao_ObjGradientEval,tao,X,G,NULL);CHKERRQ(ierr);
    tao->nfuncgrads++;
  } else if (tao->ops->computeobjective && tao->ops->computegradient) {
    ierr = PetscLogEventBegin(Tao_ObjectiveEval,tao,X,NULL,NULL);CHKERRQ(ierr);
    PetscStackPush("Tao user objective evaluation routine");
    ierr = (*tao->ops->computeobjective)(tao,X,f,tao->user_objP);CHKERRQ(ierr);
    PetscStackPop;
    ierr = PetscLogEventEnd(Tao_ObjectiveEval,tao,X,NULL,NULL);CHKERRQ(ierr);
    tao->nfuncs++;
    ierr = PetscLogEventBegin(Tao_GradientEval,tao,X,G,NULL);CHKERRQ(ierr);
    PetscStackPush("Tao user gradient evaluation routine");
    ierr = (*tao->ops->computegradient)(tao,X,G,tao->user_gradP);CHKERRQ(ierr);
    PetscStackPop;
    ierr = PetscLogEventEnd(Tao_GradientEval,tao,X,G,NULL);CHKERRQ(ierr);
    tao->ngrads++;
  } else SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONGSTATE,"TaoSetObjectiveRoutine() or TaoSetGradientRoutine() not set");
  ierr = PetscInfo1(tao,"TAO Function evaluation: %20.19e\n",(double)(*f));CHKERRQ(ierr);
  ierr = VecLockPop(X);CHKERRQ(ierr);

  ierr = TaoTestGradient(tao,G);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  TaoSetObjectiveRoutine - Sets the function evaluation routine for minimization

  Logically collective on Tao

  Input Parameter:
+ tao - the Tao context
. func - the objective function
- ctx - [optional] user-defined context for private data for the function evaluation
        routine (may be NULL)

  Calling sequence of func:
$      func (Tao tao, Vec x, PetscReal *f, void *ctx);

+ x - input vector
. f - function value
- ctx - [optional] user-defined function context

  Level: beginner

.seealso: TaoSetGradientRoutine(), TaoSetHessianRoutine() TaoSetObjectiveAndGradientRoutine()
@*/
PetscErrorCode TaoSetObjectiveRoutine(Tao tao, PetscErrorCode (*func)(Tao, Vec, PetscReal*,void*),void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  tao->user_objP = ctx;
  tao->ops->computeobjective = func;
  PetscFunctionReturn(0);
}

/*@C
  TaoSetSeparableObjectiveRoutine - Sets the function evaluation routine for least-square applications

  Logically collective on Tao

  Input Parameter:
+ tao - the Tao context
. func - the objective function evaluation routine
- ctx - [optional] user-defined context for private data for the function evaluation
        routine (may be NULL)

  Calling sequence of func:
$      func (Tao tao, Vec x, Vec f, void *ctx);

+ x - input vector
. f - function value vector
- ctx - [optional] user-defined function context

  Level: beginner

.seealso: TaoSetObjectiveRoutine(), TaoSetJacobianRoutine()
@*/
PetscErrorCode TaoSetSeparableObjectiveRoutine(Tao tao, Vec sepobj, PetscErrorCode (*func)(Tao, Vec, Vec, void*),void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  PetscValidHeaderSpecific(sepobj, VEC_CLASSID,2);
  tao->user_sepobjP = ctx;
  tao->sep_objective = sepobj;
  tao->ops->computeseparableobjective = func;
  PetscFunctionReturn(0);
}

/*@
  TaoSetSeparableObjectiveWeights - Give weights for the separable objective values. A vector can be used if only diagonal terms are used, otherwise a matrix can be give. If this function is not used, or if sigma_v and sigma_w are both NULL, then the default identity matrix will be used for weights.

  Collective on Tao

  Input Parameters:
+ tao - the Tao context
. sigma_v - vector of weights (diagonal terms only)
. n       - the number of weights (if using off-diagonal)
. rows    - index list of rows for sigma_w
. cols    - index list of columns for sigma_w
- vals - array of weights



  Note: Either sigma_v or sigma_w (or both) should be NULL

  Level: intermediate

.seealso: TaoSetSeparableObjectiveRoutine()
@*/
PetscErrorCode TaoSetSeparableObjectiveWeights(Tao tao, Vec sigma_v, PetscInt n, PetscInt *rows, PetscInt *cols, PetscReal *vals)
{
  PetscErrorCode ierr;
  PetscInt       i;
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  ierr = VecDestroy(&tao->sep_weights_v);CHKERRQ(ierr);
  tao->sep_weights_v=sigma_v;
  if (sigma_v) {
    ierr = PetscObjectReference((PetscObject)sigma_v);CHKERRQ(ierr);
  }
  if (vals) {
    if (tao->sep_weights_n) {
      ierr = PetscFree(tao->sep_weights_rows);CHKERRQ(ierr);
      ierr = PetscFree(tao->sep_weights_cols);CHKERRQ(ierr);
      ierr = PetscFree(tao->sep_weights_w);CHKERRQ(ierr);
    }
    ierr = PetscMalloc1(n,&tao->sep_weights_rows);CHKERRQ(ierr);
    ierr = PetscMalloc1(n,&tao->sep_weights_cols);CHKERRQ(ierr);
    ierr = PetscMalloc1(n,&tao->sep_weights_w);CHKERRQ(ierr);
    tao->sep_weights_n=n;
    for (i=0;i<n;i++) {
      tao->sep_weights_rows[i]=rows[i];
      tao->sep_weights_cols[i]=cols[i];
      tao->sep_weights_w[i]=vals[i];
    }
  } else {
    tao->sep_weights_n=0;
    tao->sep_weights_rows=0;
    tao->sep_weights_cols=0;
  }
  PetscFunctionReturn(0);
}
/*@
  TaoComputeSeparableObjective - Computes a separable objective function vector at a given point (for least-square applications)

  Collective on Tao

  Input Parameters:
+ tao - the Tao context
- X - input vector

  Output Parameter:
. f - Objective vector at X

  Notes:
    TaoComputeSeparableObjective() is typically used within minimization implementations,
  so most users would not generally call this routine themselves.

  Level: advanced

.seealso: TaoSetSeparableObjectiveRoutine()
@*/
PetscErrorCode TaoComputeSeparableObjective(Tao tao, Vec X, Vec F)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  PetscValidHeaderSpecific(X,VEC_CLASSID,2);
  PetscValidHeaderSpecific(F,VEC_CLASSID,3);
  PetscCheckSameComm(tao,1,X,2);
  PetscCheckSameComm(tao,1,F,3);
  if (tao->ops->computeseparableobjective) {
    ierr = PetscLogEventBegin(Tao_ObjectiveEval,tao,X,NULL,NULL);CHKERRQ(ierr);
    PetscStackPush("Tao user separable objective evaluation routine");
    ierr = (*tao->ops->computeseparableobjective)(tao,X,F,tao->user_sepobjP);CHKERRQ(ierr);
    PetscStackPop;
    ierr = PetscLogEventEnd(Tao_ObjectiveEval,tao,X,NULL,NULL);CHKERRQ(ierr);
    tao->nfuncs++;
  } else SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_WRONGSTATE,"TaoSetSeparableObjectiveRoutine() has not been called");
  ierr = PetscInfo(tao,"TAO separable function evaluation.\n");CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  TaoSetGradientRoutine - Sets the gradient evaluation routine for minimization

  Logically collective on Tao

  Input Parameter:
+ tao - the Tao context
. func - the gradient function
- ctx - [optional] user-defined context for private data for the gradient evaluation
        routine (may be NULL)

  Calling sequence of func:
$      func (Tao tao, Vec x, Vec g, void *ctx);

+ x - input vector
. g - gradient value (output)
- ctx - [optional] user-defined function context

  Level: beginner

.seealso: TaoSetObjectiveRoutine(), TaoSetHessianRoutine() TaoSetObjectiveAndGradientRoutine()
@*/
PetscErrorCode TaoSetGradientRoutine(Tao tao,  PetscErrorCode (*func)(Tao, Vec, Vec, void*),void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  tao->user_gradP = ctx;
  tao->ops->computegradient = func;
  PetscFunctionReturn(0);
}

/*@C
  TaoSetObjectiveAndGradientRoutine - Sets a combined objective function and gradient evaluation routine for minimization

  Logically collective on Tao

  Input Parameter:
+ tao - the Tao context
. func - the gradient function
- ctx - [optional] user-defined context for private data for the gradient evaluation
        routine (may be NULL)

  Calling sequence of func:
$      func (Tao tao, Vec x, PetscReal *f, Vec g, void *ctx);

+ x - input vector
. f - objective value (output)
. g - gradient value (output)
- ctx - [optional] user-defined function context

  Level: beginner

.seealso: TaoSetObjectiveRoutine(), TaoSetHessianRoutine() TaoSetObjectiveAndGradientRoutine()
@*/
PetscErrorCode TaoSetObjectiveAndGradientRoutine(Tao tao, PetscErrorCode (*func)(Tao, Vec, PetscReal *, Vec, void*), void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  tao->user_objgradP = ctx;
  tao->ops->computeobjectiveandgradient = func;
  PetscFunctionReturn(0);
}

/*@
  TaoIsObjectiveDefined -- Checks to see if the user has
  declared an objective-only routine.  Useful for determining when
  it is appropriate to call TaoComputeObjective() or
  TaoComputeObjectiveAndGradient()

  Collective on Tao

  Input Parameter:
+ tao - the Tao context
- ctx - PETSC_TRUE if objective function routine is set by user,
        PETSC_FALSE otherwise
  Level: developer

.seealso: TaoSetObjectiveRoutine(), TaoIsGradientDefined(), TaoIsObjectiveAndGradientDefined()
@*/
PetscErrorCode TaoIsObjectiveDefined(Tao tao, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  if (tao->ops->computeobjective == 0) *flg = PETSC_FALSE;
  else *flg = PETSC_TRUE;
  PetscFunctionReturn(0);
}

/*@
  TaoIsGradientDefined -- Checks to see if the user has
  declared an objective-only routine.  Useful for determining when
  it is appropriate to call TaoComputeGradient() or
  TaoComputeGradientAndGradient()

  Not Collective

  Input Parameter:
+ tao - the Tao context
- ctx - PETSC_TRUE if gradient routine is set by user, PETSC_FALSE otherwise
  Level: developer

.seealso: TaoSetGradientRoutine(), TaoIsObjectiveDefined(), TaoIsObjectiveAndGradientDefined()
@*/
PetscErrorCode TaoIsGradientDefined(Tao tao, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  if (tao->ops->computegradient == 0) *flg = PETSC_FALSE;
  else *flg = PETSC_TRUE;
  PetscFunctionReturn(0);
}

/*@
  TaoIsObjectiveAndGradientDefined -- Checks to see if the user has
  declared a joint objective/gradient routine.  Useful for determining when
  it is appropriate to call TaoComputeObjective() or
  TaoComputeObjectiveAndGradient()

  Not Collective

  Input Parameter:
+ tao - the Tao context
- ctx - PETSC_TRUE if objective/gradient routine is set by user, PETSC_FALSE otherwise
  Level: developer

.seealso: TaoSetObjectiveAndGradientRoutine(), TaoIsObjectiveDefined(), TaoIsGradientDefined()
@*/
PetscErrorCode TaoIsObjectiveAndGradientDefined(Tao tao, PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  if (tao->ops->computeobjectiveandgradient == 0) *flg = PETSC_FALSE;
  else *flg = PETSC_TRUE;
  PetscFunctionReturn(0);
}
