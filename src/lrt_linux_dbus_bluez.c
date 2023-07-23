#include "lrt_linux_dbus_bluez.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/time.h>
#include <dbus/dbus.h>


#define NUM_MS_IN_S   (1000)
#define NUM_US_IN_MS  (1000)


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


static void vLdbWriteControl(libruuvitag_context_type* px_full_ctx, uint8_t u8_control_val)
{
  write(px_full_ctx->x_ldb.i_evl_control_write_fd, &u8_control_val, sizeof(u8_control_val));
}


static dbus_bool_t tLdbAddWatch(DBusWatch* px_dbus_watch, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_watch* px_event_watch_iterator = NULL;
  lrt_ldb_watch* px_event_watch_last = NULL;
  lrt_ldb_watch* px_event_watch_new = NULL;
  void* pv_malloc_test = NULL;

  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  
  if (!dbus_watch_get_enabled(px_dbus_watch))
  {
    return TRUE;
  }
  printf("Enabled watch add called\n");

  // Check first if they exist in the list for some reason
  px_event_watch_iterator = px_full_ctx->x_ldb.px_event_watches;
  
  while (px_event_watch_iterator != NULL)
  {
    if (px_event_watch_iterator->px_dbus_watch == px_dbus_watch)
    {
      // Not updating fds, etc. Should be already.
      return TRUE;
    }
    // Moving on
    px_event_watch_last = px_event_watch_iterator;
    px_event_watch_iterator = px_event_watch_iterator->px_next_watch;
  }
  // Need to add new.
  pv_malloc_test = malloc(sizeof(lrt_ldb_watch));

  if (pv_malloc_test == NULL)
  {
    return FALSE;
  }
  px_event_watch_new = (lrt_ldb_watch*)(pv_malloc_test);
  px_event_watch_new->e_watch_type = dbus_watch_get_flags(px_dbus_watch);
  px_event_watch_new->i_watch_fd = dbus_watch_get_unix_fd(px_dbus_watch);
  px_event_watch_new->px_dbus_watch = px_dbus_watch;
  px_event_watch_new->px_next_watch = NULL;
  
  if (px_event_watch_last == NULL)
  {
    // To be added as first
    px_full_ctx->x_ldb.px_event_watches = px_event_watch_new;
  }
  else
  {
    // To be added as last
    px_event_watch_last->px_next_watch = px_event_watch_new;
  }
  // Finally enable
  px_event_watch_new->t_enabled = TRUE;
  vLdbWriteControl(px_full_ctx, LDB_CONTROL_DBUS_WATCHES);

  return TRUE;
}


static void vLdbRemoveWatch(DBusWatch* px_dbus_watch, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_watch* px_event_watch_iterator = NULL;
  lrt_ldb_watch* px_event_watch_last = NULL;
  
  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  px_event_watch_iterator = px_full_ctx->x_ldb.px_event_watches;
  
  printf("Watch remove called\n");
  
  while (px_event_watch_iterator != NULL)
  {
    if (px_event_watch_iterator->px_dbus_watch == px_dbus_watch)
    {
      // Found, just now need to remove it
      if (px_event_watch_last == NULL)
      {
        // We are removing head
        px_full_ctx->x_ldb.px_event_watches->px_next_watch = px_event_watch_iterator->px_next_watch;
      }
      else
      {
        // We are removing from middle
        px_event_watch_last->px_next_watch = px_event_watch_iterator->px_next_watch;
      }
      // Actual free
      free(px_event_watch_iterator);
      vLdbWriteControl(px_full_ctx, LDB_CONTROL_DBUS_WATCHES);

      return;
    }
    // Still here, so not found
    px_event_watch_last = px_event_watch_iterator;
    px_event_watch_iterator = px_event_watch_iterator->px_next_watch;
  }

  return;
}


static void vLdbToggleWatch(DBusWatch* px_dbus_watch, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_watch* px_event_watch_iterator = NULL;

  printf("Watch toggle called\n");
  
  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  px_event_watch_iterator = px_full_ctx->x_ldb.px_event_watches;
  
  while (px_event_watch_iterator != NULL)
  {
    if (px_event_watch_iterator->px_dbus_watch == px_dbus_watch)
    {
      // Found
      // Togling if different
      if ((px_event_watch_iterator->t_enabled) != (dbus_watch_get_enabled(px_dbus_watch)))
      {
        px_event_watch_iterator->t_enabled = dbus_watch_get_enabled(px_dbus_watch);
        vLdbWriteControl(px_full_ctx, LDB_CONTROL_DBUS_WATCHES);

        return;
      }
    }
    // Still here, so not found
    px_event_watch_iterator = px_event_watch_iterator->px_next_watch;
  }

  return;
}


static dbus_bool_t tLdbAddTimeout(DBusTimeout* px_dbus_timeout, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_timeout* px_event_timeout_iterator = NULL;
  lrt_ldb_timeout* px_event_timeout_last = NULL;
  lrt_ldb_timeout* px_event_timeout_new = NULL;
  void* pv_malloc_test = NULL;
  struct timeval x_time_now;

  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  
  if (!dbus_timeout_get_enabled(px_dbus_timeout))
  {
    return TRUE;
  }
  printf("Enabled timeout add called\n");

  // Check first if they exist in the list for some reason
  px_event_timeout_iterator = px_full_ctx->x_ldb.px_event_timeouts;
  
  while (px_event_timeout_iterator != NULL)
  {
    if (px_event_timeout_iterator->px_dbus_timeout == px_dbus_timeout)
    {
      break;
    }
    // Moving on
    px_event_timeout_last = px_event_timeout_iterator;
    px_event_timeout_iterator = px_event_timeout_iterator->px_next_timeout;
  }

  if (px_event_timeout_iterator == NULL)
  {
    // Need to add new.
    pv_malloc_test = malloc(sizeof(lrt_ldb_timeout));

    if (pv_malloc_test == NULL)
    {
      return FALSE;
    }
    px_event_timeout_new = (lrt_ldb_timeout*)(pv_malloc_test);
    px_event_timeout_new->px_dbus_timeout = px_dbus_timeout;
    px_event_timeout_new->px_next_timeout = NULL;

    // Assing impartially
    if (px_full_ctx->x_ldb.px_event_timeouts == NULL)
    {
      // Need to add as first to list
      px_full_ctx->x_ldb.px_event_timeouts = px_event_timeout_new;
    }
    else
    {
      // Need to add as last
      px_event_timeout_last->px_next_timeout = px_event_timeout_new;
    }
    // Lets make iterator to point to the newest add for some synergies
    px_event_timeout_iterator = px_event_timeout_new;
  }
  // px_event_timeout_iterator is now proper, we can do now stuff
  px_event_timeout_iterator->t_enabled = FALSE; // Just in case for a short file
  px_event_timeout_iterator->x_interval.tv_sec =
    (dbus_timeout_get_interval(px_dbus_timeout) / NUM_MS_IN_S);
  px_event_timeout_iterator->x_interval.tv_usec =
    ((dbus_timeout_get_interval(px_dbus_timeout) % NUM_MS_IN_S) *
     NUM_US_IN_MS);
  gettimeofday(&(x_time_now), NULL);
  timeradd(&(px_event_timeout_iterator->x_interval), &x_time_now,
           &(px_event_timeout_iterator->x_next_deadline));
  // Finally enable
  px_event_timeout_iterator->t_enabled = TRUE;
  vLdbWriteControl(px_full_ctx, LDB_CONTROL_DBUS_TIMEOUTS);

  return TRUE;
}


static void vLdbRemoveTimeout(DBusTimeout* px_dbus_timeout, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_timeout* px_event_timeout_iterator = NULL;
  lrt_ldb_timeout* px_event_timeout_last = NULL;

  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;

  printf("Timeout remove called\n");
  
  while (px_event_timeout_iterator != NULL)
  {
    if (px_event_timeout_iterator->px_dbus_timeout == px_dbus_timeout)
    {
      // Found, just now need to remove it
      if (px_event_timeout_last == NULL)
      {
        // We are removing head
        px_full_ctx->x_ldb.px_event_timeouts->px_next_timeout = px_event_timeout_iterator->px_next_timeout;
      }
      else
      {
        // We are removing from middle
        px_event_timeout_last->px_next_timeout = px_event_timeout_iterator->px_next_timeout;
      }
      // Actual free
      free(px_event_timeout_iterator);
      vLdbWriteControl(px_full_ctx, LDB_CONTROL_DBUS_TIMEOUTS);
      
      return;
    }
    // Moving on
    px_event_timeout_last = px_event_timeout_iterator;
    px_event_timeout_iterator = px_event_timeout_iterator->px_next_timeout;
  }


  return;
}


static void vLdbToggleTimeout(DBusTimeout* px_dbus_timeout, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_timeout* px_event_timeout_iterator = NULL;
  struct timeval x_time_now;

  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;

  printf("Timeout toggle called\n");
  
  while (px_event_timeout_iterator != NULL)
  {
    if (px_event_timeout_iterator->px_dbus_timeout == px_dbus_timeout)
    {
      // Found
      // Toggling if different
      if ((px_event_timeout_iterator->t_enabled) != (dbus_timeout_get_enabled(px_dbus_timeout)))
      {
        if (dbus_timeout_get_enabled(px_dbus_timeout) == TRUE)
        {
          // Need to set the whole thing
          px_event_timeout_iterator->x_interval.tv_sec =
            (dbus_timeout_get_interval(px_dbus_timeout) / NUM_MS_IN_S);
          px_event_timeout_iterator->x_interval.tv_usec =
            ((dbus_timeout_get_interval(px_dbus_timeout) % NUM_MS_IN_S) *
             NUM_US_IN_MS);
          gettimeofday(&(x_time_now), NULL);
          timeradd(&(px_event_timeout_iterator->x_interval), &x_time_now,
                   &(px_event_timeout_iterator->x_next_deadline));
          // Finally enable
          px_event_timeout_iterator->t_enabled = TRUE;
        }
        else
        {
          px_event_timeout_iterator->t_enabled = FALSE;
        }
        vLdbWriteControl(px_full_ctx, LDB_CONTROL_DBUS_TIMEOUTS);
      }
      return;
    }
    // Still here, so not found
    px_event_timeout_iterator = px_event_timeout_iterator->px_next_timeout;
  }

  return;
}


static uint8_t u8LdbInitDbus(libruuvitag_context_type* px_full_ctx)
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
    if (dbus_connection_set_watch_functions(px_full_ctx->x_ldb.px_dbus_conn,
                                            tLdbAddWatch,
                                            vLdbRemoveWatch,
                                            vLdbToggleWatch,
                                            px_full_ctx,
                                            NULL))
    {
      printf("Set watch succeeded\n");
    }
    else
    {
      printf("Set watch failed\n");
    }
    /*
    if (dbus_connection_set_timeout_functions(px_full_ctx->x_ldb.px_dbus_conn,
                                              tLdbAddTimeout,
                                              vLdbRemoveTimeout,
                                              vLdbToggleTimeout,
                                              px_full_ctx,
                                              NULL))
    {
      printf("Set timeout succeeded\n");
    }
    else
    {
      printf("Set timeout failed\n");
    }
    //*/
  }
  else
  {
    return LDB_FAIL;
  }
  
  return LDB_SUCCESS;
}


static uint8_t u8LdbReadControl(libruuvitag_context_type* px_full_ctx)
{
  uint8_t u8_read_control;
  
  if (sizeof(u8_read_control) == read(px_full_ctx->x_ldb.i_evl_control_read_fd,
                                 &u8_read_control, sizeof(u8_read_control)))
  {
    if ((u8_read_control == LDB_CONTROL_TERMINATE) ||
        (u8_read_control == LDB_CONTROL_DBUS_WATCHES) ||
        (u8_read_control == LDB_CONTROL_DBUS_TIMEOUTS))
    {
      ; // All ok
    }
    else
    {
      u8_read_control = LDB_CONTROL_ERROR;
    }
  }

  return u8_read_control;
}


static void vFdsetsZero(fd_set* px_fd_set_read,
                        fd_set* px_fd_set_write,
                        int* pi_descriptor_limit)
{
  (*pi_descriptor_limit) = 0;

  if (px_fd_set_read)
  {
    FD_ZERO(px_fd_set_read);
  }
  if (px_fd_set_write)
  {
    FD_ZERO(px_fd_set_write);
  }
}


static void vFdsetAdd(fd_set* px_fd_set,
                      int i_fd,
                      int* pi_descriptor_limit)
{
  if ((i_fd < 0) || (px_fd_set == NULL))
  {
    return;
  }
  FD_SET(i_fd, px_fd_set);

  if ((i_fd + 1) > (*pi_descriptor_limit))
  {
    (*pi_descriptor_limit) = i_fd + 1;
  }
}


static void* vLdbEventLoopBody(void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_watch* px_event_watch_iterator = NULL;
  int ai_pipe_fds[2];
  fd_set x_read_fds;
  fd_set x_write_fds;
  int i_descriptor_limit;
  // If I read docs correctly, exception fds are not needed.
  // Strange that wpa_supplicant uses them. I am probably reading wrong.
  int i_select_res;
  uint8_t u8_evl_running = LDB_TRUE;
  uint8_t u8_read_control;

  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;

  if (pipe(ai_pipe_fds) != 0)
  {
    printf("Pipe failed\n");
  }
  px_full_ctx->x_ldb.i_evl_control_write_fd = ai_pipe_fds[1];
  px_full_ctx->x_ldb.i_evl_control_read_fd = ai_pipe_fds[0];
    
  sem_post(&(px_full_ctx->x_ldb.x_evl_sem));
  
  while (u8_evl_running == LDB_TRUE)
  {
    // Zero
    vFdsetsZero(&x_read_fds, &x_write_fds, &i_descriptor_limit);
    // Always set control
    vFdsetAdd(&x_read_fds, px_full_ctx->x_ldb.i_evl_control_read_fd, &i_descriptor_limit);

    // Set available watches
    px_event_watch_iterator = px_full_ctx->x_ldb.px_event_watches;

    while (px_event_watch_iterator != NULL)
    {
      if (px_event_watch_iterator->e_watch_type == DBUS_WATCH_READABLE)
      {
        vFdsetAdd(&x_read_fds, px_event_watch_iterator->i_watch_fd, &i_descriptor_limit);
      }
      else if (px_event_watch_iterator->e_watch_type == DBUS_WATCH_WRITABLE)
      {
        vFdsetAdd(&x_write_fds, px_event_watch_iterator->i_watch_fd, &i_descriptor_limit);
      }
      px_event_watch_iterator = px_event_watch_iterator->px_next_watch;
    }

    // Wait for happening
    i_select_res = select(i_descriptor_limit, &x_read_fds, &x_write_fds, NULL, NULL);

    
    if (i_select_res == -1)
    {
      printf("Fatal error in select()\n");
    }
    if (i_select_res > 0)
    {
      // Check control
      if (FD_ISSET(px_full_ctx->x_ldb.i_evl_control_read_fd, &x_read_fds))
      {
        u8_read_control = u8LdbReadControl(px_full_ctx);

        // Check control parameter
        if (u8_read_control == LDB_CONTROL_TERMINATE)
        {
          u8_evl_running = LDB_FALSE;
        }
        else if (u8_read_control == LDB_CONTROL_DBUS_WATCHES)
        {
          // Basically just reloop
          printf("Reconfiguring watches in event loop\n");
        }
        else if (u8_read_control == LDB_CONTROL_DBUS_TIMEOUTS)
        {
          // Basically just reloop
          printf("Reconfiguring timeouts in event loop\n");
        }
      }

      // Actual checks
      px_event_watch_iterator = px_full_ctx->x_ldb.px_event_watches;

      while (px_event_watch_iterator != NULL)
      {
        if (px_event_watch_iterator->e_watch_type == DBUS_WATCH_READABLE)
        {
          if (FD_ISSET(px_event_watch_iterator->i_watch_fd, &x_read_fds))
          {
            printf("Read fds, handling\n");
            dbus_watch_handle(px_event_watch_iterator->px_dbus_watch, DBUS_WATCH_READABLE);
            printf("Read fds, handled\n");
          }
        }
        else if (px_event_watch_iterator->e_watch_type == DBUS_WATCH_WRITABLE)
        {
          if (FD_ISSET(px_event_watch_iterator->i_watch_fd, &x_write_fds))
          {
            printf("Write fds\n");
          }
        }
        // Get next
        px_event_watch_iterator = px_event_watch_iterator->px_next_watch;
      }
    }
  }
  

  return NULL;
}

static uint8_t u8LdbInitEventLoop(libruuvitag_context_type* px_full_ctx)
{
  pthread_create(&(px_full_ctx->x_ldb.x_evl_thread), NULL, vLdbEventLoopBody, px_full_ctx);
  sem_wait(&(px_full_ctx->x_ldb.x_evl_sem)); // Need to wait until thread up

  return LDB_SUCCESS;
}

static uint8_t u8LdbInitLocalContext(libruuvitag_context_type* px_full_ctx)
{
  px_full_ctx->x_ldb.i_evl_control_write_fd = -1;
  px_full_ctx->x_ldb.i_evl_control_read_fd = -1;
  px_full_ctx->x_ldb.px_event_watches = NULL;
  sem_init(&(px_full_ctx->x_ldb.x_evl_sem), 0, 0);
  
  return LDB_SUCCESS;
}

uint8_t u8LrtInitLinuxDbusBluez(libruuvitag_context_type* px_full_ctx)
{
  u8LdbInitLocalContext(px_full_ctx);
  u8LdbInitEventLoop(px_full_ctx);
  u8LdbInitDbus(px_full_ctx);
  
  return LDB_SUCCESS;
}


uint8_t u8LrtDeinitLinuxDbusBluez(libruuvitag_context_type* px_full_ctx)
{
  // In deinit, we need to first break out of the event loop
  vLdbWriteControl(px_full_ctx, LDB_CONTROL_TERMINATE);
  pthread_join(px_full_ctx->x_ldb.x_evl_thread, NULL);
  
  return LDB_SUCCESS;
}

