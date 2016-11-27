/* prototypes of all BDDC private functions */
#if !defined(__pcbddc_private_h)
#define __pcbddc_private_h

#include <../src/ksp/pc/impls/bddc/bddcstructs.h>

/* main functions */
PetscErrorCode PCBDDCAnalyzeInterface(PC);
PetscErrorCode PCBDDCConstraintsSetUp(PC);

/* reset functions */
PetscErrorCode PCBDDCResetTopography(PC);
PetscErrorCode PCBDDCResetSolvers(PC);
PetscErrorCode PCBDDCResetCustomization(PC);

/* graph */
PETSC_EXTERN PetscErrorCode PCBDDCGraphCreate(PCBDDCGraph*);
PETSC_EXTERN PetscErrorCode PCBDDCGraphDestroy(PCBDDCGraph*);
PETSC_EXTERN PetscErrorCode PCBDDCGraphInit(PCBDDCGraph,ISLocalToGlobalMapping,PetscInt,PetscInt);
PETSC_EXTERN PetscErrorCode PCBDDCGraphReset(PCBDDCGraph);
PETSC_EXTERN PetscErrorCode PCBDDCGraphResetCSR(PCBDDCGraph);
PETSC_EXTERN PetscErrorCode PCBDDCGraphSetUp(PCBDDCGraph,PetscInt,IS,IS,PetscInt,IS[],IS);
PETSC_EXTERN PetscErrorCode PCBDDCGraphComputeConnectedComponents(PCBDDCGraph);
PETSC_EXTERN PetscErrorCode PCBDDCGraphComputeConnectedComponentsLocal(PCBDDCGraph);
PETSC_EXTERN PetscErrorCode PCBDDCGraphASCIIView(PCBDDCGraph,PetscInt,PetscViewer);
PETSC_EXTERN PetscErrorCode PCBDDCGraphGetCandidatesIS(PCBDDCGraph,PetscInt*,IS*[],PetscInt*,IS*[],IS*);
PETSC_EXTERN PetscErrorCode PCBDDCGraphRestoreCandidatesIS(PCBDDCGraph,PetscInt*,IS*[],PetscInt*,IS*[],IS*);
PETSC_EXTERN PetscErrorCode PCBDDCGraphGetDirichletDofs(PCBDDCGraph,IS*);
PETSC_EXTERN PetscErrorCode PCBDDCGraphGetDirichletDofsB(PCBDDCGraph,IS*);

/* interface for scaling operator */
PetscErrorCode PCBDDCScalingSetUp(PC);
PetscErrorCode PCBDDCScalingDestroy(PC);
PetscErrorCode PCBDDCScalingRestriction(PC,Vec,Vec);
PetscErrorCode PCBDDCScalingExtension(PC,Vec,Vec);

/* nullspace correction */
PetscErrorCode PCBDDCNullSpaceAssembleCorrection(PC,PetscBool,PetscBool);
PetscErrorCode PCBDDCNullSpaceCheckCorrection(PC,PetscBool);

/* utils */
PetscErrorCode PCBDDCComputeLocalMatrix(PC,Mat);
PetscErrorCode PCBDDCSetUpLocalWorkVectors(PC);
PetscErrorCode PCBDDCSetUpSolvers(PC);
PetscErrorCode PCBDDCSetUpLocalScatters(PC);
PetscErrorCode PCBDDCSetUpLocalSolvers(PC,PetscBool,PetscBool);
PetscErrorCode PCBDDCSetUpCorrection(PC,PetscScalar**);
PetscErrorCode PCBDDCSetUpCoarseSolver(PC,PetscScalar*);
PetscErrorCode PCBDDCComputePrimalNumbering(PC,PetscInt*,PetscInt**);
PetscErrorCode PCBDDCScatterCoarseDataBegin(PC,InsertMode,ScatterMode);
PetscErrorCode PCBDDCScatterCoarseDataEnd(PC,InsertMode,ScatterMode);
PetscErrorCode PCBDDCApplyInterfacePreconditioner(PC,PetscBool);
PetscErrorCode PCBDDCOrthonormalizeVecs(PetscInt,Vec[]);
PetscErrorCode PCBDDCSetUseExactDirichlet(PC,PetscBool);
PetscErrorCode PCBDDCSetLevel(PC,PetscInt);
PetscErrorCode PCBDDCGlobalToLocal(VecScatter,Vec,Vec,IS,IS*);
PetscErrorCode PCBDDCAdaptiveSelection(PC);
PetscErrorCode PCBDDCConsistencyCheckIS(PC,MPI_Op,IS*);
PetscErrorCode PCBDDCComputeLocalTopologyInfo(PC);
PetscErrorCode MatGetSubMatrixUnsorted(Mat,IS,IS,Mat*);
PetscErrorCode MatDetectDisconnectedComponents(Mat,PetscBool,PetscInt*,IS*[]);
PetscErrorCode MatSeqAIJCompress(Mat,Mat*);
PetscErrorCode PCBDDCReuseSolversBenignAdapt(PCBDDCReuseSolvers,Vec,Vec,PetscBool,PetscBool);
PetscErrorCode PCBDDCComputeNoNetFlux(Mat,Mat,PetscBool,IS,PCBDDCGraph,MatNullSpace*);
PetscErrorCode PCBDDCNullSpaceCreate(MPI_Comm,PetscBool,PetscInt,Vec[],MatNullSpace*);
PetscErrorCode PCBDDCNedelecSupport(PC);

/* benign subspace trick */
PetscErrorCode PCBDDCBenignPopOrPushB0(PC,PetscBool);
PetscErrorCode PCBDDCBenignGetOrSetP0(PC,Vec,PetscBool);
PetscErrorCode PCBDDCBenignDetectSaddlePoint(PC,IS*);
PetscErrorCode PCBDDCBenignCheck(PC,IS);
PetscErrorCode PCBDDCBenignShellMat(PC,PetscBool);
PetscErrorCode PCBDDCBenignRemoveInterior(PC,Vec,Vec);

/* feti-dp */
PETSC_EXTERN PetscErrorCode PCBDDCCreateFETIDPMatContext(PC,FETIDPMat_ctx*);
PETSC_EXTERN PetscErrorCode PCBDDCSetupFETIDPMatContext(FETIDPMat_ctx);
PETSC_EXTERN PetscErrorCode PCBDDCCreateFETIDPPCContext(PC,FETIDPPC_ctx*);
PETSC_EXTERN PetscErrorCode PCBDDCSetupFETIDPPCContext(Mat,FETIDPPC_ctx);
PETSC_EXTERN PetscErrorCode FETIDPPCApply(PC,Vec,Vec);
PETSC_EXTERN PetscErrorCode PCBDDCDestroyFETIDPPC(PC);
PETSC_EXTERN PetscErrorCode FETIDPMatMult(Mat,Vec,Vec);
PETSC_EXTERN PetscErrorCode PCBDDCDestroyFETIDPMat(Mat);

/* interface to SubSchurs */
PetscErrorCode PCBDDCInitSubSchurs(PC);
PetscErrorCode PCBDDCSetUpSubSchurs(PC);

/* sub schurs */
PetscErrorCode PCBDDCSubSchursCreate(PCBDDCSubSchurs*);
PetscErrorCode PCBDDCSubSchursInit(PCBDDCSubSchurs,IS,IS,PCBDDCGraph,ISLocalToGlobalMapping,PetscBool);
PetscErrorCode PCBDDCSubSchursReset(PCBDDCSubSchurs);
PetscErrorCode PCBDDCSubSchursSetUp(PCBDDCSubSchurs,Mat,Mat,PetscBool,PetscInt[],PetscInt[],PetscInt,Vec,PetscBool,PetscBool,PetscBool,PetscInt,PetscInt[],IS[],Mat,IS);

#endif

