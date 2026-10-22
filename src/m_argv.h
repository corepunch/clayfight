// m_argv.h -- command line parameters (cf. m_argv.h)

#ifndef __M_ARGV__
#define __M_ARGV__

extern int myargc;
extern char **myargv;

// Index of the parameter, or 0 if absent.
int M_CheckParm(const char *check);
// Value following a parameter, or def.
const char *M_ParmValue(const char *check, const char *def);

#endif
