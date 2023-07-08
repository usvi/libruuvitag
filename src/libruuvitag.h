#ifndef _LRT_MAIN_H_
#define _LRT_MAIN_H_

#ifdef BACKEND_LINUX_DBUS_BLUEZ
#include "lrt_linux_dbus_bluez.h"
#else
#error Invalid backend
#endif

#include <stdint.h>

#define LIBRUUVITAG_RES_OK    (0)
#define LIBRUUVITAG_RES_AGAIN (1)
#define LIBRUUVITAG_RES_FATAL (2)


typedef struct libruuvitag_context_type libruuvitag_context_type;

struct libruuvitag_context_type
{
  // Common structures
#ifdef BACKEND_LINUX_DBUS_BLUEZ
  // Linux DBUS/Bluez structures
#endif
};


libruuvitag_context_type* pxLibRuuviTagInit(char* s_listen_on_bt_adapters, char* s_listen_to_ruuvitags);

void vLibRuuviTagDeinit(libruuvitag_context_type* px_context);

#endif // #define _LRT_MAIN_H_
