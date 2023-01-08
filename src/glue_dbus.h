#ifndef _LIBRUUVITAG_GLUE_DBUS_H_
#define _LIBRUUVITAG_GLUE_DBUS_H_

#include <stdint.h>

#include <dbus/dbus.h>

uint8_t u8InitSystemDbusConnection(DBusConnection** ppx_dbus_system_conn);

#endif // #ifndef _LIBRUUVITAG_GLUE_DBUS_H_
