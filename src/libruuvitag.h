#ifndef _LIBRUUVITAG_H_
#define _LIBRUUVITAG_H_


// Main context forward declaration
typedef struct libruuvitag_context_type libruuvitag_context_type;


// Assert proper backend and include corresponding header
#ifdef BACKEND_LINUX_DBUS_BLUEZ
#include "lrt_linux_dbus_bluez.h"
#else
#error Invalid backend
#endif


// Headers needed in crafting our own top-level context type.
#include <stdint.h>


// Top-level context definition.
struct libruuvitag_context_type
{
  // Common structures

  // Backend specific structures
#ifdef BACKEND_LINUX_DBUS_BLUEZ
  lrt_ldb_context_type x_ldb; // Linux Dbus Bluez
#endif
};


// Function prototypes
libruuvitag_context_type* pxLibRuuviTagInit(char* s_listen_on_bt_adapters, char* s_listen_to_ruuvitags);
void vLibRuuviTagDeinit(libruuvitag_context_type* px_context);

#endif // #define _LIBRUUVITAG_H_
