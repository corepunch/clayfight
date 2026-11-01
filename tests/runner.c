// Small native runner: a failure ends its test, and every test still runs.
#include "test.h"
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static struct {const char *name;void (*fn)(void);} tests[64];
static int count,checks;
static jmp_buf failed;
void test_register(const char *name,void (*fn)(void)) {if(count<64) {tests[count].name=name;tests[count++].fn=fn;}}
void test_pass(void) {checks++;}
void test_fail(const char *file,int line,const char *condition) {fprintf(stderr,"  %s:%d: CHECK(%s)\n",file,line,condition);longjmp(failed,1);}
static double now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int test_main(int argc,char **argv) {
    int passed=0,bad=0;double start=now();
    for(int i=0;i<count;i++) {
        bool any=false,wanted=false;
        for(int a=1;a<argc;a++)if(argv[a][0]!='-') {any=true;if(strstr(tests[i].name,argv[a]))wanted=true;}
        if(any&&!wanted)continue;
        if(argc>1 && !strcmp(argv[1],"--list")) {puts(tests[i].name);continue;}
        double began=now();
        if(setjmp(failed)) {printf("  FAIL %s\n",tests[i].name);bad++;}
        else {tests[i].fn();printf("  PASS %-36s %.3fs\n",tests[i].name,now()-began);passed++;}
    }
    printf("clayfight: %d passed, %d failed, %d checks, %.3fs\n",passed,bad,checks,now()-start);
    return bad!=0;
}
