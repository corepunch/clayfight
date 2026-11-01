#ifndef CLAY_TEST_H
#define CLAY_TEST_H
#include <math.h>
#include <stdbool.h>
void test_register(const char *name,void (*fn)(void));
void test_pass(void);
_Noreturn void test_fail(const char *file,int line,const char *condition);
int test_main(int argc,char **argv);
#define CHECK(c) ((c)?test_pass():test_fail(__FILE__,__LINE__,#c))
#define NEAR(a,b) CHECK(fabsf((float)(a)-(float)(b))<.001f)
#define TEST(name) static void test_##name(void); \
 __attribute__((constructor)) static void register_##name(void) {test_register(#name,test_##name);} \
 static void test_##name(void)
#endif
