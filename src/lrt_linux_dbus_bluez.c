#include "lrt_linux_dbus_bluez.h"

#include <stdio.h>
#include <inttypes.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/time.h>
#include <sys/epoll.h>
#include <dbus/dbus.h>


#define NUM_MS_IN_S   (1000)
#define NUM_US_IN_MS  (1000)

#define LDB_TRUE           (1)
#define LDB_FALSE          (0)
#define LDB_UNKNOWN        (2)

#define LDB_SEM_INIT_FAILED                (-1)
#define LDB_FD_INVALID                     (-1)
#define LDB_TIMEOUT_INVALID                (-1)
#define LDB_EPOLL_OP_INVALID               (-1)

#define LDB_EPOLL_CREATE1_DEFAULTS         (0)
#define LDB_EPOLL_EVENT_FLAGS_NONE         (0)
#define LDB_EPOLL_MONITOR_NUM_EVENTS       (10)

#define LDB_CONTROL_MAIN_OP_TERMINATE      (1)
#define LDB_CONTROL_MAIN_OP_WATCH          (2)
#define LDB_CONTROL_MAIN_OP_TIMEOUT        (3)

#define LDB_CONTROL_NO_OP                  (0)

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
#define LDB_INITED_FLAGS_EPOLL_CREATED           (((uint32_t)1) << 1)
#define LDB_INITED_FLAGS_EPOLL_CTRL_PIPE_ADDED   (((uint32_t)1) << 2)
#define LDB_INITED_FLAGS_CONN                    (((uint32_t)1) << 3)
#define LDB_INITED_FLAGS_IFACES_ADDED            (((uint32_t)1) << 4)
#define LDB_INITED_FLAGS_IFACES_REMOVED          (((uint32_t)1) << 5)
#define LDB_INITED_FLAGS_WATCHES_ADDED           (((uint32_t)1) << 6)
#define LDB_INITED_FLAGS_TIMEOUTS_ADDED          (((uint32_t)1) << 7)
#define LDB_INITED_FLAGS_CORELOOP_RUNNING        (((uint32_t)1) << 8)



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


static void vLdbWriteControl(libruuvitag_context_type* px_full_ctx,
			     uint8_t u8_main_op,
			     int i_epoll_op,
                             uint32_t u32_epoll_event_flags,
			     int i_file_descriptor)
{
  write(px_full_ctx->x_ldb.i_evl_control_write_fd, &u8_main_op, sizeof(uint8_t));

  if (i_epoll_op != LDB_EPOLL_OP_INVALID)
  {
    write(px_full_ctx->x_ldb.i_evl_control_write_fd, &i_epoll_op, sizeof(int));
    write(px_full_ctx->x_ldb.i_evl_control_write_fd, &u32_epoll_event_flags, sizeof(uint32_t));
    write(px_full_ctx->x_ldb.i_evl_control_write_fd, &i_file_descriptor, sizeof(int));
  }
}


static uint8_t u8LdbReadControl(libruuvitag_context_type* px_full_ctx,
                                uint8_t* pu8_main_op,
                                int* pi_epoll_op,
                                uint32_t* pu32_epoll_event_flags,
                                int* pi_file_descriptor)
{
  if (pu8_main_op != NULL)
  {
    *pu8_main_op = LDB_CONTROL_NO_OP;
  }
  if (pi_epoll_op != NULL)
  {
    *pi_epoll_op = LDB_EPOLL_OP_INVALID;
  }
  if (pu32_epoll_event_flags != NULL)
  {
    *pu32_epoll_event_flags = LDB_EPOLL_EVENT_FLAGS_NONE;
  }
  if (pi_file_descriptor != NULL)
  {
    *pi_file_descriptor = LDB_FD_INVALID;
  }
  
  if (sizeof(uint8_t) == read(px_full_ctx->x_ldb.i_evl_control_read_fd,
                                 pu8_main_op, sizeof(uint8_t)))
  {
    if ((*pu8_main_op) == LDB_CONTROL_MAIN_OP_TERMINATE)
    {
      return LDB_SUCCESS;
    }
    else if ((*pu8_main_op) == LDB_CONTROL_MAIN_OP_WATCH)
    {
      if ((sizeof(int) == read(px_full_ctx->x_ldb.i_evl_control_read_fd,
                               pi_epoll_op, sizeof(int))) &&
	  (sizeof(uint32_t) == read(px_full_ctx->x_ldb.i_evl_control_read_fd,
                                    pu32_epoll_event_flags, sizeof(uint32_t))) &&
	  (sizeof(int) == read(px_full_ctx->x_ldb.i_evl_control_read_fd,
                               pi_file_descriptor, sizeof(int))))
      {
	return LDB_SUCCESS;
      }
    }
    else if ((*pu8_main_op) == LDB_CONTROL_MAIN_OP_TIMEOUT)
    {
      return LDB_SUCCESS;
    }
  }    
  
  return LDB_FAIL;
}


static dbus_bool_t tLdbAddWatch(DBusWatch* px_dbus_watch, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_watch* px_event_watch_iterator = NULL;
  lrt_ldb_watch* px_event_watch_last = NULL;
  lrt_ldb_watch* px_event_watch_new = NULL;
  void* pv_malloc_test = NULL;
  int i_epoll_op = LDB_EPOLL_OP_INVALID;
  int i_search_fd = LDB_FD_INVALID;

  printf("Watch add called\n");

  // Add all kinds of watches, also disabled. If DBus instructs us
  // to add them disabled, there is a reason for it.

  // Actually search for file descriptors because of epoll
  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  i_search_fd = dbus_watch_get_unix_fd(px_dbus_watch);
  px_event_watch_iterator = px_full_ctx->x_ldb.px_event_watches;
  
  while (px_event_watch_iterator != NULL)
  {
    if (px_event_watch_iterator->i_watch_fd == i_search_fd)
    {
      break;
    }
    // Moving on
    px_event_watch_last = px_event_watch_iterator;
    px_event_watch_iterator = px_event_watch_iterator->px_next_watch;
  }

  if (px_event_watch_iterator == NULL)
  {
    printf("Allocating new watch container\n");
    i_epoll_op = EPOLL_CTL_ADD;
    // Add new placeholder
    pv_malloc_test = malloc(sizeof(lrt_ldb_watch));

    if (pv_malloc_test == NULL)
    {
      return FALSE;
    }
    px_event_watch_new = (lrt_ldb_watch*)(pv_malloc_test);
    px_event_watch_new->i_watch_fd = dbus_watch_get_unix_fd(px_dbus_watch);
    px_event_watch_new->u32_epoll_event_flags = LDB_EPOLL_EVENT_FLAGS_NONE;
    px_event_watch_new->px_dbus_read_watch = NULL;
    px_event_watch_new->px_dbus_write_watch = NULL;
    px_event_watch_new->px_next_watch = NULL;

    if (px_event_watch_last == NULL)
    {
      // Add as first
      px_full_ctx->x_ldb.px_event_watches = px_event_watch_new;
    }
    else
    {
      // Add as last
      px_event_watch_last->px_next_watch = px_event_watch_new;
    }
    // Set iterator to same as the case where the placeholder is existing
    px_event_watch_iterator = px_event_watch_new;
  }
  else
  { 
    i_epoll_op = EPOLL_CTL_MOD;
  }
  if (dbus_watch_get_flags(px_dbus_watch) == DBUS_WATCH_READABLE)
  {
    px_event_watch_iterator->px_dbus_read_watch = px_dbus_watch;
    px_event_watch_iterator->u32_epoll_event_flags |= EPOLLIN;
  }
  else if (dbus_watch_get_flags(px_dbus_watch) == DBUS_WATCH_READABLE)
  {
    px_event_watch_iterator->px_dbus_write_watch = px_dbus_watch;
    px_event_watch_iterator->u32_epoll_event_flags |= EPOLLOUT;
  }
  if (i_epoll_op != LDB_EPOLL_OP_INVALID)
  {
    vLdbWriteControl(px_full_ctx,
                     LDB_CONTROL_MAIN_OP_WATCH,
                     i_epoll_op,
                     px_event_watch_iterator->u32_epoll_event_flags,
                     px_event_watch_iterator->i_watch_fd);

    return TRUE;
  }

  // Something strange has happened, return false
  return FALSE;
}


static void vLdbRemoveWatch(DBusWatch* px_dbus_watch, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_watch* px_event_watch_iterator = NULL;
  lrt_ldb_watch* px_event_watch_last = NULL;
  // Need to keep op and flags as separate copy in
  // case we are removing the entry
  int i_epoll_op = LDB_EPOLL_OP_INVALID;
  uint32_t u32_epoll_event_flags = LDB_EPOLL_EVENT_FLAGS_NONE;
  int i_search_fd = LDB_FD_INVALID;

  printf("Watch remove called\n");

  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  i_search_fd = dbus_watch_get_unix_fd(px_dbus_watch);
  px_event_watch_iterator = px_full_ctx->x_ldb.px_event_watches;
  
  while (px_event_watch_iterator != NULL)
  {
    if (px_event_watch_iterator->i_watch_fd == i_search_fd)
    {
      printf("Match in native remove\n");
      // Just need to figure out which to remove and if we need to remove the container also
      if (px_event_watch_iterator->px_dbus_read_watch == px_dbus_watch)
      {
	px_event_watch_iterator->px_dbus_read_watch = NULL;
	i_epoll_op = EPOLL_CTL_MOD; // Override later if removing the container
        px_event_watch_iterator->u32_epoll_event_flags &= ~EPOLLIN;        
        u32_epoll_event_flags = px_event_watch_iterator->u32_epoll_event_flags;
      }
      else if (px_event_watch_iterator->px_dbus_write_watch == px_dbus_watch)
      {
	px_event_watch_iterator->px_dbus_write_watch = NULL;
	i_epoll_op = EPOLL_CTL_MOD; // Override later if removing the container
        px_event_watch_iterator->u32_epoll_event_flags &= ~EPOLLOUT;
        u32_epoll_event_flags = px_event_watch_iterator->u32_epoll_event_flags;
      }
      // Remove completely if both are null now
      if ((px_event_watch_iterator->px_dbus_read_watch == NULL) &&
	  (px_event_watch_iterator->px_dbus_write_watch == NULL))
      {
	// Final event removed, removing container
	printf("Removing watch container");
	i_epoll_op = EPOLL_CTL_DEL; // Overridden

	if (px_event_watch_last == NULL)
	{
	  // We are removing head
	  px_full_ctx->x_ldb.px_event_watches = px_event_watch_iterator->px_next_watch;
	}
	else
	{
	  // We are removing from middle
	  px_event_watch_last->px_next_watch = px_event_watch_iterator->px_next_watch;
	}
	// Actual free
	free(px_event_watch_iterator);
      }
      
      // Parameters have been recorded to flags, if valid
      if (i_epoll_op != LDB_EPOLL_OP_INVALID)
      {
        vLdbWriteControl(px_full_ctx,
                         LDB_CONTROL_MAIN_OP_WATCH,
                         i_epoll_op,
                         u32_epoll_event_flags,
			 i_search_fd);

        return;
      }
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
  int i_search_fd = LDB_FD_INVALID;

  printf("Watch toggle called\n");
  
  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  i_search_fd = dbus_watch_get_unix_fd(px_dbus_watch);
  px_event_watch_iterator = px_full_ctx->x_ldb.px_event_watches;
  
  while (px_event_watch_iterator != NULL)
  {
    if (px_event_watch_iterator->i_watch_fd == i_search_fd)
    {
      if (dbus_watch_get_flags(px_dbus_watch) == DBUS_WATCH_READABLE)
      {
        if (dbus_watch_get_enabled(px_dbus_watch))
        {
          px_event_watch_iterator->u32_epoll_event_flags |= EPOLLIN;
        }
        else
        {
          px_event_watch_iterator->u32_epoll_event_flags &= ~EPOLLIN;
        }
      }
      else if (dbus_watch_get_flags(px_dbus_watch) == DBUS_WATCH_WRITABLE)
      {
        if (dbus_watch_get_enabled(px_dbus_watch))
        {
          px_event_watch_iterator->u32_epoll_event_flags |= EPOLLOUT;
        }
        else
        {
          px_event_watch_iterator->u32_epoll_event_flags &= ~EPOLLOUT;
        }
      }
      vLdbWriteControl(px_full_ctx,
                       LDB_CONTROL_MAIN_OP_WATCH,
                       EPOLL_CTL_MOD,
                       px_event_watch_iterator->u32_epoll_event_flags,
                       px_event_watch_iterator->i_watch_fd);
      
      return;
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

  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  
  // Same here, add timeout even if it is disabled.
  printf("Timeout add called\n");

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
  if (dbus_timeout_get_enabled(px_event_timeout_iterator->px_dbus_timeout) == TRUE)
  {
    px_event_timeout_iterator->i_timeout_left =
      dbus_timeout_get_interval(px_event_timeout_iterator->px_dbus_timeout);
  }
  else
  {
    px_event_timeout_iterator->i_timeout_left = LDB_TIMEOUT_INVALID;
  }
  vLdbWriteControl(px_full_ctx,
                   LDB_CONTROL_MAIN_OP_TIMEOUT,
                   LDB_EPOLL_OP_INVALID,
                   LDB_EPOLL_EVENT_FLAGS_NONE,
                   LDB_FD_INVALID);

  return TRUE;
}


static void vLdbRemoveTimeout(DBusTimeout* px_dbus_timeout, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_timeout* px_event_timeout_iterator = NULL;
  lrt_ldb_timeout* px_event_timeout_last = NULL;

  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  px_event_timeout_iterator = px_full_ctx->x_ldb.px_event_timeouts;

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
      vLdbWriteControl(px_full_ctx,
                       LDB_CONTROL_MAIN_OP_TIMEOUT,
                       LDB_EPOLL_OP_INVALID,
                       LDB_EPOLL_EVENT_FLAGS_NONE,
                       LDB_FD_INVALID);
      
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

  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  px_event_timeout_iterator = px_full_ctx->x_ldb.px_event_timeouts;

  printf("Timeout toggle called\n");
  
  while (px_event_timeout_iterator != NULL)
  {
    if (px_event_timeout_iterator->px_dbus_timeout == px_dbus_timeout)
    {
      // Found. Toggling if different
      if ((dbus_timeout_get_enabled(px_dbus_timeout) == TRUE) &&
          (dbus_timeout_get_enabled(px_event_timeout_iterator->px_dbus_timeout) == FALSE))
      {
        px_event_timeout_iterator->i_timeout_left =
          dbus_timeout_get_interval(px_event_timeout_iterator->px_dbus_timeout);
        vLdbWriteControl(px_full_ctx,
                         LDB_CONTROL_MAIN_OP_TIMEOUT,
                         LDB_EPOLL_OP_INVALID,
                         LDB_EPOLL_EVENT_FLAGS_NONE,
                         LDB_FD_INVALID);
      }
      else if ((dbus_timeout_get_enabled(px_dbus_timeout) == FALSE) &&
          (dbus_timeout_get_enabled(px_event_timeout_iterator->px_dbus_timeout) == TRUE))
      {
        px_event_timeout_iterator->i_timeout_left = LDB_TIMEOUT_INVALID;
        vLdbWriteControl(px_full_ctx,
                         LDB_CONTROL_MAIN_OP_TIMEOUT,
                         LDB_EPOLL_OP_INVALID,
                         LDB_EPOLL_EVENT_FLAGS_NONE,
                         LDB_FD_INVALID);
      }
      // As this was the timeout, return regardless of if we changed anything
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
  struct epoll_event x_epoll_temp_event;
  
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

  // Remove control pipe from epoll
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_EPOLL_CTRL_PIPE_ADDED)
  {
    printf("Removing control pipe from epoll\n");
    x_epoll_temp_event.events = 0;
    x_epoll_temp_event.data.fd = px_full_ctx->x_ldb.i_evl_control_read_fd;
    epoll_ctl(px_full_ctx->x_ldb.i_epoll_fd, EPOLL_CTL_DEL,
              x_epoll_temp_event.data.fd, &x_epoll_temp_event);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_EPOLL_CTRL_PIPE_ADDED;
  }

  // Remove the epoll structure itself
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_EPOLL_CREATED)
  {
    printf("Removing epoll structure\n");
    close(px_full_ctx->x_ldb.i_epoll_fd);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_EPOLL_CREATED;
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



static void* vLdbEventLoopBody(void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_watch* px_event_watch_iterator = NULL;
  int ai_pipe_fds[2];
  // If I read docs correctly, exception fds are not needed.
  // Strange that wpa_supplicant uses them. I am probably reading wrong.
  uint8_t u8_evl_running = LDB_TRUE;

  struct epoll_event x_epoll_temp_event;
  struct epoll_event ax_epoll_monitor_events[LDB_EPOLL_MONITOR_NUM_EVENTS];
  int i_epoll_fds_ready = 0;
  int i_epoll_iterator = 0;
  int i_next_timeout = LDB_TIMEOUT_INVALID;

  uint8_t u8_main_op = LDB_CONTROL_NO_OP;
  int i_epoll_op = LDB_EPOLL_OP_INVALID;
  uint32_t u32_epoll_event_flags = LDB_EPOLL_EVENT_FLAGS_NONE;
  int i_control_passed_fd = LDB_FD_INVALID;
  
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

  // Try to create and set the epoll stuff
  px_full_ctx->x_ldb.i_epoll_fd = epoll_create1(LDB_EPOLL_CREATE1_DEFAULTS);

  if (px_full_ctx->x_ldb.i_epoll_fd == LDB_FD_INVALID)
  {
    printf("Unable to create epoll fd\n");
    vLdbDeinitEventLoopWithDbus(px_full_ctx);
    sem_post(&(px_full_ctx->x_ldb.x_inited_sem));

    return NULL;
  }
  px_full_ctx->x_ldb.u32_inited_flags |= LDB_INITED_FLAGS_EPOLL_CREATED;
  
  // Add the control pipe to epoll interest list. Rest is added inside the loop.
  x_epoll_temp_event.events = EPOLLIN;
  x_epoll_temp_event.data.fd = px_full_ctx->x_ldb.i_evl_control_read_fd;

  if (epoll_ctl(px_full_ctx->x_ldb.i_epoll_fd, EPOLL_CTL_ADD,
                x_epoll_temp_event.data.fd, &x_epoll_temp_event) == LDB_EPOLL_OP_INVALID)
  {
    printf("Unable to add control fd to epoll\n");
    vLdbDeinitEventLoopWithDbus(px_full_ctx);
    sem_post(&(px_full_ctx->x_ldb.x_inited_sem));

    return NULL;
  }
  px_full_ctx->x_ldb.u32_inited_flags |= LDB_INITED_FLAGS_EPOLL_CTRL_PIPE_ADDED;
  
  // Setting up the actual dbus
  if (u8LdbInitDbus(px_full_ctx) != LDB_SUCCESS)
  {
    printf("Underlying dbus initialization has failed\n");
    vLdbDeinitEventLoopWithDbus(px_full_ctx);
    sem_post(&(px_full_ctx->x_ldb.x_inited_sem));

    return NULL;
  }
  px_full_ctx->x_ldb.u32_inited_flags |= LDB_INITED_FLAGS_CORELOOP_RUNNING;

  // Finally we can release caller
  sem_post(&(px_full_ctx->x_ldb.x_inited_sem));
  
  while (u8_evl_running == LDB_TRUE)
  {
    sleep(1);
    i_epoll_fds_ready = epoll_wait(px_full_ctx->x_ldb.i_epoll_fd, ax_epoll_monitor_events,
                                   LDB_EPOLL_MONITOR_NUM_EVENTS, i_next_timeout);
    printf("Epoll got %d fds\n", i_epoll_fds_ready);

    for (i_epoll_iterator = 0; i_epoll_iterator < i_epoll_fds_ready; i_epoll_iterator++)
    {
      printf(" %d\n", ax_epoll_monitor_events[i_epoll_iterator].data.fd);

      if (ax_epoll_monitor_events[i_epoll_iterator].data.fd ==
          px_full_ctx->x_ldb.i_evl_control_read_fd)
      {
        if (u8LdbReadControl(px_full_ctx, &u8_main_op, &i_epoll_op,
                             &u32_epoll_event_flags, &i_control_passed_fd) == LDB_SUCCESS)
        {
          if (u8_main_op == LDB_CONTROL_MAIN_OP_TERMINATE)
          {
            printf("Terminating as requested\n");
            u8_evl_running = LDB_FALSE;
            
            break;
          }
        }
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
  px_full_ctx->x_ldb.i_evl_control_write_fd = LDB_FD_INVALID;
  px_full_ctx->x_ldb.i_evl_control_read_fd = LDB_FD_INVALID;
  px_full_ctx->x_ldb.i_epoll_fd = LDB_FD_INVALID;
  px_full_ctx->x_ldb.px_event_watches = NULL;
  px_full_ctx->x_ldb.px_event_timeouts = NULL;
  px_full_ctx->x_ldb.u32_inited_flags = LDB_INITED_FLAGS_NONE;


  if (sem_init(&(px_full_ctx->x_ldb.x_inited_sem), 0, 0) == LDB_SEM_INIT_FAILED)
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
    vLdbWriteControl(px_full_ctx,
                     LDB_CONTROL_MAIN_OP_TERMINATE,
                     LDB_EPOLL_OP_INVALID,
                     LDB_EPOLL_EVENT_FLAGS_NONE,
                     LDB_FD_INVALID);

    // Event loop cleans up automatically after it terminates
    pthread_join(px_full_ctx->x_ldb.x_evl_thread, NULL);
  }
  printf("Returning success\n");
  
  return LDB_SUCCESS;
}

