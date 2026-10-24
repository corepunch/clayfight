// Local capture server, launched without shell interpolation.
#include <SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "platform/platform.h"

static pid_t child;
static char readyfile[128], localurl[300], phoneurl[300];

int I_CreatorStatus(void)
{
    if (child <= 0) return -1;
    int status;
    pid_t state = waitpid(child, &status, WNOHANG);
    if (state == child || (state < 0 && errno == ECHILD)) { child = 0; return -1; }
    if (localurl[0]) return 1;
    FILE *f = fopen(readyfile, "r");
    if (!f) return 0;
    bool ok = fgets(localurl, sizeof localurl, f) && fgets(phoneurl, sizeof phoneurl, f);
    fclose(f);
    if (!ok) { localurl[0] = 0; return 0; }
    localurl[strcspn(localurl, "\r\n")] = 0;
    phoneurl[strcspn(phoneurl, "\r\n")] = 0;
    return 1;
}

bool I_StartCreator(const char *creatures, const char *data)
{
    if (I_CreatorStatus() >= 0) return true;
    snprintf(readyfile, sizeof readyfile, "build/capture-%ld.url", (long)getpid());
    unlink(readyfile);
    localurl[0] = phoneurl[0] = 0;
    child = fork();
    if (child < 0) { child = 0; return false; }
    if (!child) {
        int fd = open("build/capture.log", O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (fd >= 0) { dup2(fd, STDOUT_FILENO); dup2(fd, STDERR_FILENO); close(fd); }
        execlp("python3", "python3", "tools/capture_server.py", "--port", "0", "--ready", readyfile,
               "--creatures", creatures, "--data", data, (char *)NULL);
        _exit(127);
    }
    return true;
}

const char *I_CreatorURL(bool phone) { return phone ? phoneurl : localurl; }
void I_OpenCreator(void)
{
    if (I_CreatorStatus() == 1) {
        if (SDL_OpenURL(localurl) != 0) I_Log("Could not open capture browser: %s", SDL_GetError());
    }
}
void I_CloseCreator(void)
{
    if (child > 0) { kill(child, SIGTERM); waitpid(child, NULL, WNOHANG); child = 0; }
    if (readyfile[0]) unlink(readyfile);
}
