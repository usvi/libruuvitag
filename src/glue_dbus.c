#include "glue_dbus.h"

#include "libruuvitag.h"

#include <stdio.h>

#include <dbus/dbus.h>


uint8_t u8InitSystemDbusConnection(DBusConnection** ppx_dbus_system_conn)
{
  DBusError x_dbus_error;

  dbus_error_init(&x_dbus_error);
  *ppx_dbus_system_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &x_dbus_error);

  if (*ppx_dbus_system_conn == NULL)
  {
    return LIBRUUVITAG_RES_FATAL;
  }
  if (dbus_error_is_set(&x_dbus_error))
  {
    return LIBRUUVITAG_RES_FATAL;
  }

  return LIBRUUVITAG_RES_OK;
}


