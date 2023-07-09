#ifndef _LRT_LINUX_DBUS_BLUEZ_H_
#define _LRT_LINUX_DBUS_BLUEZ_H_


// Headers needed in crafting our own dbus struct type.
#include <inttypes.h>
#include <dbus/dbus.h>


// Careful definition ordering to keep top-level header
// happy and at the same time define strictly the dbus-
// related stuff here.
typedef struct lrt_ldb_context_type lrt_ldb_context_type;

struct lrt_ldb_context_type
{
  DBusConnection* px_dbus_conn;
};


// Top-level header
#include "libruuvitag.h"


// Normal defines
#define LDB_SUCCESS        (1)
#define LDB_FAIL           (0)
#define LDB_AGAIN          (2)



// Function prototypes
uint8_t u8LrtInitLinuxDbusBluez(libruuvitag_context_type* px_full_context);
uint8_t u8LrtDeinitLinuxDbusBluez(libruuvitag_context_type* px_full_context);

#endif // #ifndef _LRT_LINUX_DBUS_BLUEZ_H_

