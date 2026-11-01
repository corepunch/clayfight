#include "platform/platform.h"
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if !defined(__EMSCRIPTEN__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
static pid_t child;
static char readyfile[256],localurl[512],phoneurl[512];
static char script[1024]="tools/capture_server.py", executable[1024]="build/clayfight";
void clay_creator_config(const char *s,const char *e) {snprintf(script,sizeof script,"%s",s);snprintf(executable,sizeof executable,"%s",e);}
int I_CreatorStatus(void) {
    if(child<=0)return -1;
    int status;pid_t state=waitpid(child,&status,WNOHANG);
    if(state==child||(state<0&&errno==ECHILD)){child=0;return -1;}
    if(localurl[0])return 1;
    FILE *f=fopen(readyfile,"r");if(!f)return 0;
    bool ok=fgets(localurl,sizeof localurl,f)&&fgets(phoneurl,sizeof phoneurl,f);fclose(f);
    if(!ok){localurl[0]=0;return 0;}
    localurl[strcspn(localurl,"\r\n")]=0;phoneurl[strcspn(phoneurl,"\r\n")]=0;return 1;
}
bool I_StartCreator(const char *creatures,const char *data) {
    if(I_CreatorStatus()>=0)return true;
    snprintf(readyfile,sizeof readyfile,"/tmp/clay-capture-%ld.url",(long)getpid());unlink(readyfile);localurl[0]=phoneurl[0]=0;
    child=fork();if(child<0){child=0;return false;}
    if(!child){execlp("python3","python3",script,"--port","8765","--ready",readyfile,"--creatures",creatures,"--data",data,"--executable",executable,(char*)NULL);_exit(127);}return true;
}
const char *I_CreatorURL(bool phone){return phone?phoneurl:localurl;}
void I_OpenCreator(void) {
    if(I_CreatorStatus()!=1)return;
    pid_t p=fork();if(!p){
#ifdef __APPLE__
        execl("/usr/bin/open","open",localurl,(char*)NULL);
#else
        execlp("xdg-open","xdg-open",localurl,(char*)NULL);
#endif
        _exit(127);
    }
    if(p>0)waitpid(p,NULL,0);
}
void I_CloseCreator(void) {if(child>0){kill(child,SIGTERM);waitpid(child,NULL,0);child=0;}if(readyfile[0])unlink(readyfile);}
#elif defined(__EMSCRIPTEN__)
void clay_creator_config(const char *s,const char *e){}
bool I_StartCreator(const char *c,const char *d){return false;}
int I_CreatorStatus(void){return -1;}
const char *I_CreatorURL(bool p){return "Native app required for camera capture";}
void I_OpenCreator(void){}
void I_CloseCreator(void){}
#endif
