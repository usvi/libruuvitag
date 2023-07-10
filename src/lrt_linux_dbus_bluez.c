#include "lrt_linux_dbus_bluez.h"

#include <stdio.h>
#include <unistd.h>
#include <dbus/dbus.h>


/*
static DBusHandlerResult tInterfacesAltered(DBusConnection* px_dbus_conn,
                                            DBusMessage* px_dbus_msg,
                                            void* pv_full_ctx)
{
  libruuvitag_context_type* px_full_context = (libruuvitag_context_type*)pv_full_ctx;

  printf("Interfaces possibly added or removed\n");

  return DBUS_HANDLER_RESULT_HANDLED;
}
*/



static uint8_t u8LrtInitIpc(libruuvitag_context_type* px_full_ctx)
{
  DBusError x_error;

  dbus_error_init(&x_error);
  px_full_ctx->x_ldb.px_dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &x_error);

  if (dbus_error_is_set(&x_error))
  {
    printf("Bus failed\n");
    dbus_error_free(&x_error);

    return LDB_FAIL;
  }
  else
  {

  }
  if (px_full_ctx->x_ldb.px_dbus_conn)
  {
    printf("Bus succeeded\n");
    /*
    dbus_connection_add_filter(px_full_ctx->x_ldb.px_dbus_conn,
                               tInterfacesAltered, px_full_ctx, NULL);
    */
    printf("Adding signal match\n");
    dbus_bus_add_match(px_full_ctx->x_ldb.px_dbus_conn,
                       "type='signal',"
                       "interface='org.freedesktop.DBus.ObjectManager',"
                       "member='InterfacesAdded',"
                       "path='/'",
                       &x_error);
    dbus_connection_flush(px_full_ctx->x_ldb.px_dbus_conn);
    printf("Flush succeeded\n");

    if (dbus_error_is_set(&x_error))
    {
      printf("Setting match has errored\n");
    }

  }
  else
  {
    return LDB_FAIL;
  }
  
  return LDB_SUCCESS;
}

static void* vLrtEventLoopBody(void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  int ai_pipe_fds[2];
  fd_set x_read_fds;
  int i_control_read_fd;
  int i_select_res;
  uint8_t u8_control;

  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;

  if (pipe(ai_pipe_fds) != 0)
  {
    printf("Pipe failed\n");
  }
  px_full_ctx->x_ldb.i_evl_control_write_fd = ai_pipe_fds[1];
  i_control_read_fd = ai_pipe_fds[0];

  // Set up the file descriptors
  FD_ZERO(&x_read_fds);
  FD_SET(i_control_read_fd, &x_read_fds);
  px_full_ctx->x_ldb.i_evl_descriptor_limit = i_control_read_fd + 1;
  sem_post(&(px_full_ctx->x_ldb.x_evl_sem));
  
  while (1)
  {
    sleep(1); // Just in case
    i_select_res = select(px_full_ctx->x_ldb.i_evl_descriptor_limit,
                          &x_read_fds, NULL, NULL, NULL);
    
    if (i_select_res == -1)
    {
      printf("Fatal error in select()\n");
    }
    if (i_select_res > 0)
    {
      if (FD_ISSET(i_control_read_fd, &x_read_fds))
      {
        if (sizeof(u8_control) == read(i_control_read_fd, &u8_control, sizeof(u8_control)))
        {
          if (u8_control == LDB_CONTROL_TERMINATE)
          {
            break;
          }
        }
      }
    }
  }

  return NULL;
}

static uint8_t u8LrtInitEventLoop(libruuvitag_context_type* px_full_ctx)
{
  sem_init(&(px_full_ctx->x_ldb.x_evl_sem), 0, 0);
  pthread_create(&(px_full_ctx->x_ldb.x_evl_thread), NULL, vLrtEventLoopBody, px_full_ctx);
  sem_wait(&(px_full_ctx->x_ldb.x_evl_sem)); // Need to wait until thread up

  return LDB_SUCCESS;
}


uint8_t u8LrtInitLinuxDbusBluez(libruuvitag_context_type* px_full_ctx)
{
  u8LrtInitEventLoop(px_full_ctx);
  
  return LDB_SUCCESS;
}


uint8_t u8LrtDeinitLinuxDbusBluez(libruuvitag_context_type* px_full_ctx)
{
  // Write to control, then join
  uint8_t u8_control = LDB_CONTROL_TERMINATE;
  write(px_full_ctx->x_ldb.i_evl_control_write_fd, &u8_control, 1);
  pthread_join(px_full_ctx->x_ldb.x_evl_thread, NULL);
  
  return LDB_SUCCESS;
}

