
CLFAGS          =
FFLAGS	        =
CPPFLAGS        =
FPPFLAGS        =
LOCDIR          = src/ts/examples/tutorials/advection-diffusion-reaction/
EXAMPLESC       = ex1.c ex2.c ex3.c ex4.c ex5.c ex5opt_ic.c
EXAMPLESF       = shashi.F90
EXAMPLESFH      =
MANSEC          = TS
DIRS            =
CLEANFILES      = TS-data-*

include ${PETSC_DIR}/lib/petsc/conf/variables
include ${PETSC_DIR}/lib/petsc/conf/rules

shashi: shashi.o  chkopts
	-${FLINKER} -o shashi shashi.o  ${PETSC_TS_LIB}
	${RM} shashi.o

include ${PETSC_DIR}/lib/petsc/conf/test
