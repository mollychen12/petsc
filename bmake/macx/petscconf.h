#ifdef PETSC_RCS_HEADER
"$Id: petscconf.h,v 1.1 2000/04/18 23:26:42 balay Exp bsmith $"
"Defines the configuration for this machine"
#endif

#if !defined(INCLUDED_PETSCCONF_H)
#define INCLUDED_PETSCCONF_H


#define PARCH_mac-x
#define PETSC_ARCH_NAME "mac-x"

#define PETSC_HAVE_POPEN

#define PETSC_HAVE_SYS_WAIT_H 1
#define PETSC_HAVE_VPRINTF 1
#define PETSC_RETSIGTYPE void
#define PETSC_STDC_HEADERS 1
#define PETSC_TIME_WITH_SYS_TIME 1
#define PETSC_WORDS_BIGENDIAN 1
#define PETSC_ARCH_NAME "beos"
#define PETSC_SIZEOF_INT 4
#define PETSC_SIZEOF_VOID_P 4
#define PETSC_HAVE_GETCWD 1
#define PETSC_HAVE_GETHOSTNAME 1
#define PETSC_HAVE_GETTIMEOFDAY 1
#define PETSC_HAVE_GETWD 1
#define PETSC_HAVE_MEMMOVE 1
#define PETSC_HAVE_RAND 1
#define PETSC_HAVE_READLINK 1
#define PETSC_HAVE_REALPATH 1
#define PETSC_HAVE_SIGACTION 1
#define PETSC_HAVE_SIGNAL 1
#define PETSC_HAVE_SOCKET 1
#define PETSC_HAVE_STRSTR 1
#define PETSC_HAVE_UNAME 1
#define PETSC_HAVE_FCNTL_H 1
#define PETSC_HAVE_LIMITS_H 1
#define PETSC_HAVE_PWD_H 1
#define PETSC_HAVE_STDLIB_H 1
#define PETSC_HAVE_STRING_H 1
#define PETSC_HAVE_STRINGS_H 1
#define PETSC_HAVE_SYS_RESOURCE_H 1
#define PETSC_HAVE_SYS_TIME_H 1
#define PETSC_HAVE_UNISTD_H 1


#define PETSC_HAVE_FORTRAN_UNDERSCORE 
#define PETSC_HAVE_FORTRAN_UNDERSCORE_UNDERSCORE

#define PETSC_HAVE_DOUBLE_ALIGN_MALLOC
#define PETSC_CANNOT_START_DEBUGGER
#define PETSC_HAVE_NO_GETRUSAGE


#endif

