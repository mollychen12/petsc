/*$Id: cstringbase.c,v 1.1 2000/02/02 23:09:35 bsmith Exp bsmith $*/
#include "petsc.h"

#define NOTLIKELY -1.234567890123

EXTERN_C_BEGIN
#undef __FUNC__  
#define __FUNC__ "PFApply_String"
int PFApply_String(void *value,int n,Scalar *in,Scalar *out)
{
  int       i;
  Scalar    x,y,z,f = NOTLIKELY,x1,x2,x3,x4,x5,f1,f2,f3,f4,f5;

  PetscFunctionBegin;
  for (i=0; i<n; i++) {
    x1 = x = in[_MIN_*i];
#if (_MIN_ > 1) 
    x2 = y = in[_MIN_*i+1];
#endif
#if (_MIN_ > 2)
    x3 = z = in[_MIN_*i+2];
#endif
#if (_MIN_ > 3)
    x4 = in[_MIN_*i+3];
#endif
#if (_MIN_ > 4)
    x5 = in[_MIN_*i+4];
#endif
    FUNCTION;
    if (f == NOTLIKELY) {
      out[_NOUT_*i] = f1;
    } else {
      out[_NOUT_*i] = f;
    }
#if (_NOUT_ > 1) 
    out[_NOUT_*i+1] = f2; 
#endif
#if (_NOUT_ > 2) 
    out[_NOUT_*i+2] = f3;
#endif
#if (_NOUT_ > 3) 
    out[_NOUT_*i+3] = f4;
#endif
#if (_NOUT_ > 4) 
    out[_NOUT_*i+4] = f5;
#endif
  }
  PetscFunctionReturn(0);
}
EXTERN_C_END
