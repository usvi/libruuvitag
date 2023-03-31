#ifndef _LRT_MAIN_H_
#define _LRT_MAIN_H_

#include <stdint.h>

#define LIBRUUVITAG_RES_OK    (0)
#define LIBRUUVITAG_RES_AGAIN (1)
#define LIBRUUVITAG_RES_FATAL (2)



uint8_t u8LibRuuviTagInit(char* s_listen_on, char* s_listen_to);

uint8_t u8LibRuuviTagDeinit(void);

#endif // #define _LRT_MAIN_H_
