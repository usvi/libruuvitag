#include "lrt_dbus.h"

#include "libruuvitag.h"
#include "lrt_context.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <dbus/dbus.h>



// Lets put this to context once we are otherwise done

static void* pvEventLoopRoutine(void* pv_data)
{
  lrt_context_type* px_lrt_context = (lrt_context_type*)pv_data;

  px_lrt_context->px_dbus->u8_event_loop_running = 1;

  while (px_lrt_context->px_dbus->u8_event_loop_running)
  {
    printf("Thread\n");
    sleep(1);
  }

  return LIBRUUVITAG_RES_OK;
}


static uint8_t u8LrtInitEventLoop(lrt_context_type* px_lrt_context)
{
  pthread_create(&(px_lrt_context->px_dbus->x_event_loop_thread),
                 NULL, pvEventLoopRoutine, px_lrt_context);
  // FIXME: If fails, set running = 0

  return LIBRUUVITAG_RES_OK;
}


uint8_t u8LrtInitDbus(lrt_context_type* px_lrt_context)
{
  DBusError x_dbus_error;
  void* pv_malloc_test;

  pv_malloc_test = malloc(sizeof(*(px_lrt_context->px_dbus)));

  if (pv_malloc_test == NULL)
  {
    return LIBRUUVITAG_RES_FATAL;
  }
  px_lrt_context->px_dbus = (lrt_dbus_type*)pv_malloc_test;
  memset(px_lrt_context->px_dbus, 0, sizeof(*(px_lrt_context->px_dbus)));

  dbus_error_init(&x_dbus_error);
  px_lrt_context->px_dbus->px_sys_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &x_dbus_error);

  // If needed, add a filter like:
  // dbus_connection_add_filter(priv->con, disconnect_filter, priv,
  // NULL);
  
  if (px_lrt_context->px_dbus->px_sys_conn == NULL)
  {
    dbus_error_free(&x_dbus_error);
    free(px_lrt_context->px_dbus);
    printf("FATAL A\n");
    
    return LIBRUUVITAG_RES_FATAL;
  }
  if (dbus_error_is_set(&x_dbus_error))
  {
    dbus_error_free(&x_dbus_error);
    free(px_lrt_context->px_dbus);
    printf("FATAL B\n");

    return LIBRUUVITAG_RES_FATAL;
  }
  dbus_error_free(&x_dbus_error);

  // Lets put this somewhere when we know better:
  u8LrtInitEventLoop(px_lrt_context);

  return LIBRUUVITAG_RES_OK;
}

void vLrtDeinitDbus(lrt_context_type* px_lrt_context)
{
  dbus_connection_unref(px_lrt_context->px_dbus->px_sys_conn);
  
  if (px_lrt_context->px_dbus->u8_event_loop_running)
  {
    pthread_cancel(px_lrt_context->px_dbus->x_event_loop_thread);
  }

  if (px_lrt_context->px_dbus != NULL)
  {
    free(px_lrt_context->px_dbus);
    px_lrt_context->px_dbus = NULL;
  }
}
