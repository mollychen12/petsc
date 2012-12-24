static char help[] = "Tests 1D discretization tools.\n\n";

#include <petscdt.h>

#undef __FUNCT__
#define __FUNCT__ "CheckPoints"
static PetscErrorCode CheckPoints(const char *name,PetscInt npoints,const PetscReal *points,PetscInt ndegrees,const PetscInt *degrees)
{
  PetscErrorCode ierr;
  PetscReal *B,*D,*D2;
  PetscInt i,j;

  PetscFunctionBegin;
  ierr = PetscMalloc3(npoints*ndegrees,PetscReal,&B,npoints*ndegrees,PetscReal,&D,npoints*ndegrees,PetscReal,&D2);CHKERRQ(ierr);
  ierr = PetscDTLegendreEval(npoints,points,ndegrees,degrees,B,D,D2);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"%s\n",name);CHKERRQ(ierr);
  for (i=0; i<npoints; i++) {
    for (j=0; j<ndegrees; j++) {
      ierr = PetscPrintf(PETSC_COMM_WORLD,"degree %D at %12.4G: B=%12.4G  D=%12.4G  D2=%12.4G\n",degrees[j],points[i],B[i*ndegrees+j],D[i*ndegrees+j],D2[i*ndegrees+j]);CHKERRQ(ierr);
    }
  }
  ierr = PetscFree3(B,D,D2);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}



#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  PetscErrorCode ierr;
  PetscInt degrees[1000],ndegrees,npoints,two;
  PetscReal points[1000],weights[1000],interval[2];
  PetscBool flg;

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr);
  ierr = PetscOptionsBegin(PETSC_COMM_WORLD,PETSC_NULL,"Discretization tools test options",PETSC_NULL);CHKERRQ(ierr);
  {
    ndegrees = 1000;
    degrees[0] = 0;
    degrees[1] = 1;
    degrees[2] = 2;
    ierr = PetscOptionsIntArray("-degrees","list of degrees to evaluate","",degrees,&ndegrees,&flg);CHKERRQ(ierr);
    if (!flg) ndegrees = 3;
    npoints = 1000;
    points[0] = 0.0;
    points[1] = -0.5;
    points[2] = 1.0;
    ierr = PetscOptionsRealArray("-points","list of points at which to evaluate","",points,&npoints,&flg);CHKERRQ(ierr);
    if (!flg) npoints = 3;
    two = 2;
    interval[0] = -1.;
    interval[1] = 1.;
    ierr = PetscOptionsRealArray("-interval","interval on which to construct quadrature","",interval,&two,PETSC_NULL);CHKERRQ(ierr);
  }
  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  ierr = CheckPoints("User-provided points",npoints,points,ndegrees,degrees);CHKERRQ(ierr);

  ierr = PetscDTGaussQuadrature(npoints,interval[0],interval[1],points,weights);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Quadrature weights\n");CHKERRQ(ierr);
  ierr = PetscRealView(npoints,weights,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  {
    PetscReal a = interval[0],b = interval[1],zeroth,first,second;
    PetscInt i;
    zeroth = b - a;
    first = (b*b - a*a)/2;
    second = (b*b*b - a*a*a)/3;
    for (i=0; i<npoints; i++) {
      zeroth -= weights[i];
      first -= weights[i] * points[i];
      second -= weights[i] * PetscSqr(points[i]);
    }
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Moment error: zeroth=%G, first=%G, second=%G\n",-zeroth,-first,-second);CHKERRQ(ierr);
  }
  ierr = CheckPoints("Gauss points",npoints,points,ndegrees,degrees);CHKERRQ(ierr);
  ierr = PetscFinalize();
  return 0;
}

