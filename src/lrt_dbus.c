#include "lrt_dbus.h"

#include "libruuvitag.h"
#include "lrt_context.h"

#include <stdio.h>
#include <dbus/dbus.h>



// Lets put this to context once we are otherwise done

void* pvEventLoopRoutine(void* pv_data)
{

  return NULL;
}

uint8_t u8LrtInitDbus(lrt_context_type* px_lrt_context)
{
  DBusError x_dbus_error;

  dbus_error_init(&x_dbus_error);
  px_lrt_context->px_dbus->px_sys_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &x_dbus_error);

  // If needed, add a filter like:
  // dbus_connection_add_filter(priv->con, disconnect_filter, priv,
  // NULL);
  
  if (px_lrt_context->px_dbus->px_sys_conn == NULL)
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

void vLrtDeinitDbus(lrt_context_type* px_lrt_context)
{
  dbus_connection_unref(px_lrt_context->px_dbus->px_sys_conn);
}
