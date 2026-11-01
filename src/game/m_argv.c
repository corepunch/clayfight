// m_argv.c -- command line parameters (cf. m_argv.c)

#include <strings.h>

#include "game/resources.h"

int myargc;
char **myargv;

int M_CheckParm(const char *check)
{
    for (int i = 1; i < myargc; i++)
        if (!strcasecmp(check, myargv[i]))
            return i;
    return 0;
}

const char *M_ParmValue(const char *check, const char *def)
{
    int i = M_CheckParm(check);
    return i && i + 1 < myargc ? myargv[i + 1] : def;
}
