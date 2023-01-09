#include "glue_dbus.h"

#include "libruuvitag.h"

#include <stdio.h>

#include <dbus/dbus.h>


uint8_t u8InitSystemDbusConnection(DBusConnection** ppx_dbus_system_conn)
{
  DBusError x_dbus_error;

  dbus_error_init(&x_dbus_error);
  *ppx_dbus_system_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &x_dbus_error);

  // If needed, add a filter like:
  // dbus_connection_add_filter(priv->con, disconnect_filter, priv,
  // NULL);
  
  if (*ppx_dbus_system_conn == NULL)
  {
    dbus_error_free(&x_dbus_error);
    
    return LIBRUUVITAG_RES_FATAL;
  }
  if (dbus_error_is_set(&x_dbus_error))
  {
    dbus_error_free(&x_dbus_error);

    return LIBRUUVITAG_RES_FATAL;
  }
  dbus_error_free(&x_dbus_error);

  return LIBRUUVITAG_RES_OK;
}

void vDeinitSystemDbusConnection(DBusConnection** ppx_dbus_system_conn)
{
  dbus_connection_unref(*ppx_dbus_system_conn);
}
