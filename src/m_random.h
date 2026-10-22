// m_random.h -- deterministic random numbers (cf. m_random.h)

#ifndef __M_RANDOM__
#define __M_RANDOM__

int P_Random(void); // play simulation only: 0..255
int M_Random(void); // menus and cosmetics: 0..255
void M_ClearRandom(void);

#endif
