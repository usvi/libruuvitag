#include "lrt_linux_dbus_bluez.h"

#include <stdio.h>
#include <inttypes.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/time.h>
#include <dbus/dbus.h>


#define NUM_MS_IN_S   (1000)
#define NUM_US_IN_MS  (1000)

#define LDB_SIGNAL_DEF_INTERFACES_ADDED \
  "type='signal',"\
  "interface='org.freedesktop.DBus.ObjectManager',"\
  "member='InterfacesAdded',"\
  "path='/'"

#define LDB_SIGNAL_DEF_INTERFACES_REMOVED \
  "type='signal',"\
  "interface='org.freedesktop.DBus.ObjectManager',"\
  "member='InterfacesRemoved',"\
  "path='/'"

#define LDB_INITED_FLAGS_NONE                    (((uint32_t)0) << 0)
#define LDB_INITED_FLAGS_CTRL_PIPE               (((uint32_t)1) << 0)
#define LDB_INITED_FLAGS_CONN                    (((uint32_t)1) << 1)
#define LDB_INITED_FLAGS_IFACES_ADDED            (((uint32_t)1) << 2)
#define LDB_INITED_FLAGS_IFACES_REMOVED          (((uint32_t)1) << 3)
#define LDB_INITED_FLAGS_WATCHES_ADDED           (((uint32_t)1) << 4)
#define LDB_INITED_FLAGS_TIMEOUTS_ADDED          (((uint32_t)1) << 5)
#define LDB_INITED_FLAGS_CORELOOP_RUNNING        (((uint32_t)1) << 6)


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
      printf("Match in native remove\n");
      // Found, just now need to remove it
      if (px_event_watch_last == NULL)
      {
        // We are removing head
	printf("Native remove head\n");
        px_full_ctx->x_ldb.px_event_watches = px_event_watch_iterator->px_next_watch;
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
        px_full_ctx->x_ldb.px_event_timeouts = px_event_timeout_iterator->px_next_timeout;
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

static void vLdbDeinitEventLoopWithDbus(libruuvitag_context_type* px_full_ctx)
{
  lrt_ldb_timeout* px_event_timeout_iterator = NULL;
  lrt_ldb_timeout* px_event_timeout_last = NULL;
  lrt_ldb_watch* px_event_watch_iterator = NULL;
  lrt_ldb_watch* px_event_watch_last = NULL;
  
  // Need to work backwards on the resources

  // Coreloop
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_CORELOOP_RUNNING)
  {
    printf("Deiniting coreloop\n");
    // DOes nothing, but for sanity.
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_CORELOOP_RUNNING;
  }

  // Timeouts
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_TIMEOUTS_ADDED)
  {
    printf("Deiniting timeouts\n");
    dbus_connection_set_timeout_functions(px_full_ctx->x_ldb.px_dbus_conn,
					  NULL, NULL, NULL, NULL, NULL);

    px_event_timeout_iterator = px_full_ctx->x_ldb.px_event_timeouts;

    // Release the linkedlist stuff
    while (px_event_timeout_iterator != NULL)
    {
      px_event_timeout_last = px_event_timeout_iterator;
      px_event_timeout_iterator = px_event_timeout_iterator->px_next_timeout;

      free(px_event_timeout_last);
    }
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_TIMEOUTS_ADDED;
  }

  // Watches
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_WATCHES_ADDED)
  {
    printf("Deiniting watches\n");
    dbus_connection_set_watch_functions(px_full_ctx->x_ldb.px_dbus_conn,
					  NULL, NULL, NULL, NULL, NULL);

    px_event_watch_iterator = px_full_ctx->x_ldb.px_event_watches;

    // Release the linkedlist stuff
    while (px_event_watch_iterator != NULL)
    {
      px_event_watch_last = px_event_watch_iterator;
      px_event_watch_iterator = px_event_watch_iterator->px_next_watch;

      printf("Dedicated free\n");
      free(px_event_watch_last);
    }
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_WATCHES_ADDED;
  }

  // Interfaces removed subscription
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_IFACES_REMOVED)
  {
    printf("Deiniting interfaces removed\n");
    dbus_bus_remove_match(px_full_ctx->x_ldb.px_dbus_conn,
			  LDB_SIGNAL_DEF_INTERFACES_REMOVED,
			  NULL);
    // Needs flush because error is NULL
    dbus_connection_flush(px_full_ctx->x_ldb.px_dbus_conn);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_IFACES_REMOVED;
  }
  
  // Interfaces added subscription
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_IFACES_ADDED)
  {
    printf("Deiniting interfaces added\n");
    dbus_bus_remove_match(px_full_ctx->x_ldb.px_dbus_conn,
			  LDB_SIGNAL_DEF_INTERFACES_ADDED,
			  NULL);
    // Needs flush because error is NULL
    dbus_connection_flush(px_full_ctx->x_ldb.px_dbus_conn);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_IFACES_ADDED;
  }

  // Pending calls to be done later
  
  // The actual dbus connection
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_CONN)
  {
    printf("Deiniting dbus connection\n");
    dbus_connection_flush(px_full_ctx->x_ldb.px_dbus_conn);
    dbus_connection_unref(px_full_ctx->x_ldb.px_dbus_conn);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_CONN;
  }

  // Control pipe
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_CTRL_PIPE)
  {
    printf("Deiniting control pipe fds\n");
    close(px_full_ctx->x_ldb.i_evl_control_write_fd);
    close(px_full_ctx->x_ldb.i_evl_control_read_fd);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_CTRL_PIPE;
  }
}

static uint8_t u8LdbInitDbus(libruuvitag_context_type* px_full_ctx)
{
  DBusError x_error;

  dbus_error_init(&x_error);

  // Making the connection
  px_full_ctx->x_ldb.px_dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &x_error);

  if (dbus_error_is_set(&x_error))
  {
    dbus_error_free(&x_error);
    printf("Bus failed\n");
    vLdbDeinitEventLoopWithDbus(px_full_ctx);

    return LDB_FAIL;
  }
  px_full_ctx->x_ldb.u32_inited_flags |= LDB_INITED_FLAGS_CONN;

  // Adding match for added interfaces
  dbus_bus_add_match(px_full_ctx->x_ldb.px_dbus_conn,
		     LDB_SIGNAL_DEF_INTERFACES_ADDED,
		     &x_error);
  // If we pass error, does not need flushing, sayeth the docs
  if (dbus_error_is_set(&x_error))
  {
    dbus_error_free(&x_error);
    printf("Adding InterfacesAdded match failed\n");
    vLdbDeinitEventLoopWithDbus(px_full_ctx);

    return LDB_FAIL;
  }
  px_full_ctx->x_ldb.u32_inited_flags |= LDB_INITED_FLAGS_IFACES_ADDED;

  // Adding match for removed interfaces
  dbus_bus_add_match(px_full_ctx->x_ldb.px_dbus_conn,
		     LDB_SIGNAL_DEF_INTERFACES_REMOVED,
		     &x_error);

  if (dbus_error_is_set(&x_error))
  {
    dbus_error_free(&x_error);
    printf("Adding InterfacesRemoved match failed\n");
    vLdbDeinitEventLoopWithDbus(px_full_ctx);

    return LDB_FAIL;
  }
  px_full_ctx->x_ldb.u32_inited_flags |= LDB_INITED_FLAGS_IFACES_REMOVED;

  /*
    dbus_connection_add_filter(px_full_ctx->x_ldb.px_dbus_conn,
                               tInterfacesAltered, px_full_ctx, NULL);
  */

  // Setting watches
  if (dbus_connection_set_watch_functions(px_full_ctx->x_ldb.px_dbus_conn,
					  tLdbAddWatch,
					  vLdbRemoveWatch,
					  vLdbToggleWatch,
					  px_full_ctx,
					  NULL) != TRUE)
  {
    printf("Set Watch failed\n");
    vLdbDeinitEventLoopWithDbus(px_full_ctx);

    return LDB_FAIL;
  }
  px_full_ctx->x_ldb.u32_inited_flags |= LDB_INITED_FLAGS_WATCHES_ADDED;

  // Setting timeouts
  if (dbus_connection_set_timeout_functions(px_full_ctx->x_ldb.px_dbus_conn,
                                              tLdbAddTimeout,
                                              vLdbRemoveTimeout,
                                              vLdbToggleTimeout,
                                              px_full_ctx,
                                              NULL) != TRUE)
  {
    printf("Set Watch failed\n");
    vLdbDeinitEventLoopWithDbus(px_full_ctx);

    return LDB_FAIL;
  }    
  px_full_ctx->x_ldb.u32_inited_flags |= LDB_INITED_FLAGS_TIMEOUTS_ADDED;

  // All should be fine now
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
    printf("Control pipe creation failed\n");
    vLdbDeinitEventLoopWithDbus(px_full_ctx);
    sem_post(&(px_full_ctx->x_ldb.x_inited_sem));

    return NULL;
  }
  px_full_ctx->x_ldb.i_evl_control_write_fd = ai_pipe_fds[1];
  px_full_ctx->x_ldb.i_evl_control_read_fd = ai_pipe_fds[0];
  px_full_ctx->x_ldb.u32_inited_flags |= LDB_INITED_FLAGS_CTRL_PIPE;
  
  if (u8LdbInitDbus(px_full_ctx) != LDB_SUCCESS)
  {
    printf("Underlying dbus initialization has failed\n");
    vLdbDeinitEventLoopWithDbus(px_full_ctx);
    sem_post(&(px_full_ctx->x_ldb.x_inited_sem));

    return NULL;
  }
  
  px_full_ctx->x_ldb.u32_inited_flags |= LDB_INITED_FLAGS_CORELOOP_RUNNING;
  sem_post(&(px_full_ctx->x_ldb.x_inited_sem));
  
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
  // Out of event loop, most probably because we are deinitializing.
  // Release resources.
  vLdbDeinitEventLoopWithDbus(px_full_ctx);

  return NULL;
}


static uint8_t u8LdbInitLocalContext(libruuvitag_context_type* px_full_ctx)
{
  px_full_ctx->x_ldb.i_evl_control_write_fd = -1;
  px_full_ctx->x_ldb.i_evl_control_read_fd = -1;
  px_full_ctx->x_ldb.px_event_watches = NULL;
  px_full_ctx->x_ldb.px_event_timeouts = NULL;
  px_full_ctx->x_ldb.u32_inited_flags = LDB_INITED_FLAGS_NONE;


  if (sem_init(&(px_full_ctx->x_ldb.x_inited_sem), 0, 0) == -1)
  {
    return LDB_FAIL;
  }

  return LDB_SUCCESS;
}

static uint8_t u8LdbInitEventLoopWithDbus(libruuvitag_context_type* px_full_ctx)
{
  pthread_create(&(px_full_ctx->x_ldb.x_evl_thread), NULL, vLdbEventLoopBody, px_full_ctx);
  // Need to wait here until semaphore to get the status, be it anything
  sem_wait(&(px_full_ctx->x_ldb.x_inited_sem));
  
  return LDB_SUCCESS;
}


uint8_t u8LrtInitLinuxDbusBluez(libruuvitag_context_type* px_full_ctx)
{
  if (u8LdbInitLocalContext(px_full_ctx) != LDB_SUCCESS)
  {
    return LDB_FAIL;
  }
  if (u8LdbInitEventLoopWithDbus(px_full_ctx) != LDB_SUCCESS)
  {
    return LDB_FAIL;
  }
  
  return LDB_SUCCESS;
}


uint8_t u8LrtDeinitLinuxDbusBluez(libruuvitag_context_type* px_full_ctx)
{
  // In deinit, we need to first break out of the event loop
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_CORELOOP_RUNNING)
  {
    vLdbWriteControl(px_full_ctx, LDB_CONTROL_TERMINATE);
    // Event loop cleans up automatically after it terminates
    pthread_join(px_full_ctx->x_ldb.x_evl_thread, NULL);
  }
  // Release non-dbus structures
  
  return LDB_SUCCESS;
}

