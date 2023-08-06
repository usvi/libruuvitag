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
#define LDB_CONTROL_MAIN_OP_RECEIVERS      (4)

#define LDB_CONTROL_NO_OP                  (0)

#define LDB_COMPARE_NEGATIVE               (-1)
#define LDB_COMPARE_NEUTRAL                (0)
#define LDB_COMPARE_POSITIVE               (1)

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
#define LDB_INITED_FLAGS_LINKED_LISTS            (((uint32_t)1) << 0)
#define LDB_INITED_FLAGS_CTRL_PIPE               (((uint32_t)1) << 1)
#define LDB_INITED_FLAGS_EPOLL_CREATED           (((uint32_t)1) << 2)
#define LDB_INITED_FLAGS_EPOLL_CTRL_PIPE_ADDED   (((uint32_t)1) << 3)
#define LDB_INITED_FLAGS_CONN                    (((uint32_t)1) << 4)
#define LDB_INITED_FLAGS_IFACES_ADDED            (((uint32_t)1) << 5)
#define LDB_INITED_FLAGS_IFACES_REMOVED          (((uint32_t)1) << 6)
#define LDB_INITED_FLAGS_WATCHES_ADDED           (((uint32_t)1) << 7)
#define LDB_INITED_FLAGS_TIMEOUTS_ADDED          (((uint32_t)1) << 8)
#define LDB_INITED_FLAGS_CORELOOP_RUNNING        (((uint32_t)1) << 9)



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
    else if ((*pu8_main_op) == LDB_CONTROL_MAIN_OP_RECEIVERS)
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


static int iCompareNodeAndDbusWatchFds(lrt_llist_node* px_list_node, void* pv_dbus_watch_data)
{
  if (((lrt_ldb_node_watch*)px_list_node)->i_watch_fd ==
      dbus_watch_get_unix_fd((DBusWatch*)pv_dbus_watch_data))
  {
    return LDB_COMPARE_NEUTRAL;
  }
  
  return LDB_COMPARE_NEGATIVE;
}


static dbus_bool_t tLdbAddWatch(DBusWatch* px_dbus_watch, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_node_watch* px_node_watch_this = NULL;
  lrt_ldb_node_watch* px_node_watch_new = NULL;
  
  void* pv_malloc_test = NULL;
  int i_epoll_op = LDB_EPOLL_OP_INVALID;

  printf("Watch add called\n");

  // Add all kinds of watches, also disabled. If DBus instructs us
  // to add them disabled, there is a reason for it.

  // Actually search for file descriptors because of epoll
  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;

  px_node_watch_this =
    (lrt_ldb_node_watch*)pxLrtLlistFindNode(px_full_ctx->x_ldb.px_llist_watches,
                                            LRT_LLIST_FIND_EQUAL,
                                            iCompareNodeAndDbusWatchFds,
                                            px_dbus_watch);
    
  if (px_node_watch_this == NULL)
  {
    printf("Allocating new watch container\n");
    i_epoll_op = EPOLL_CTL_ADD;
    // Add new placeholder
    pv_malloc_test = malloc(sizeof(lrt_ldb_node_watch));

    if (pv_malloc_test == NULL)
    {
      return FALSE;
    }
    px_node_watch_new = (lrt_ldb_node_watch*)(pv_malloc_test);
    px_node_watch_new->i_watch_fd = dbus_watch_get_unix_fd(px_dbus_watch);
    px_node_watch_new->u32_epoll_event_flags = LDB_EPOLL_EVENT_FLAGS_NONE;
    px_node_watch_new->px_dbus_read_watch = NULL;
    px_node_watch_new->px_dbus_write_watch = NULL;
    vLrtLlistAddNode(px_full_ctx->x_ldb.px_llist_watches,
                     px_node_watch_new);
    // Set main context as retrievable data in case we need it in handling
    // (we could do this also manually and actually previously did, but this
    // is just more convenient).
    dbus_watch_set_data(px_dbus_watch, px_full_ctx, NULL);
    // Set iterator to same as the case where the placeholder is existing
    px_node_watch_this = px_node_watch_new;
  }
  else
  { 
    i_epoll_op = EPOLL_CTL_MOD;
  }
  if (dbus_watch_get_flags(px_dbus_watch) & DBUS_WATCH_READABLE)
  {
    px_node_watch_this->px_dbus_read_watch = px_dbus_watch;

    if (dbus_watch_get_enabled(px_dbus_watch) == TRUE)
    {
      printf("Adding enabled readable watch\n");
      px_node_watch_this->u32_epoll_event_flags |= EPOLLIN;
    }
    else
    {
      printf("Adding disabled readable watch\n");
      px_node_watch_this->u32_epoll_event_flags &= ~EPOLLIN;
    }
  }
  if (dbus_watch_get_flags(px_dbus_watch) & DBUS_WATCH_WRITABLE)
  {
    px_node_watch_this->px_dbus_write_watch = px_dbus_watch;
    
    if (dbus_watch_get_enabled(px_dbus_watch) == TRUE)
    {
      printf("Adding enabled writable watch\n");
      px_node_watch_this->u32_epoll_event_flags |= EPOLLOUT;
    }
    else
    {
      printf("Adding disabled writable watch\n");
      px_node_watch_this->u32_epoll_event_flags &= ~EPOLLOUT;
    }
  }
  if (i_epoll_op != LDB_EPOLL_OP_INVALID)
  {
    vLdbWriteControl(px_full_ctx,
                     LDB_CONTROL_MAIN_OP_WATCH,
                     i_epoll_op,
                     px_node_watch_this->u32_epoll_event_flags,
                     px_node_watch_this->i_watch_fd);

    return TRUE;
  }

  // Something strange has happened, return false
  return FALSE;
}


static void vLdbRemoveWatch(DBusWatch* px_dbus_watch, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_node_watch* px_node_watch_this = NULL;
  // Need to keep op and flags as separate copy in
  // case we are removing the entry
  int i_epoll_op = LDB_EPOLL_OP_INVALID;
  uint32_t u32_epoll_event_flags = LDB_EPOLL_EVENT_FLAGS_NONE;

  printf("Watch remove called\n");
  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  px_node_watch_this =
    (lrt_ldb_node_watch*)pxLrtLlistFindNode(px_full_ctx->x_ldb.px_llist_watches,
                                            LRT_LLIST_FIND_EQUAL,
                                            iCompareNodeAndDbusWatchFds,
                                            px_dbus_watch);

  if (px_node_watch_this != NULL)
  {
    printf("Match in native remove\n");
    // Just need to figure out which to remove and if we need to remove the container also
    if (px_node_watch_this->px_dbus_read_watch == px_dbus_watch)
    {
      px_node_watch_this->px_dbus_read_watch = NULL;
      i_epoll_op = EPOLL_CTL_MOD; // Override later if removing the container
      px_node_watch_this->u32_epoll_event_flags &= ~EPOLLIN;        
      u32_epoll_event_flags = px_node_watch_this->u32_epoll_event_flags;
    }
    else if (px_node_watch_this->px_dbus_write_watch == px_dbus_watch)
    {
      px_node_watch_this->px_dbus_write_watch = NULL;
      i_epoll_op = EPOLL_CTL_MOD; // Override later if removing the container
      px_node_watch_this->u32_epoll_event_flags &= ~EPOLLOUT;
      u32_epoll_event_flags = px_node_watch_this->u32_epoll_event_flags;
    }
    // Remove completely if both are null now
    if ((px_node_watch_this->px_dbus_read_watch == NULL) &&
	  (px_node_watch_this->px_dbus_write_watch == NULL))
    {
      // Final event removed, removing container
      printf("Removing watch container\n");
      i_epoll_op = EPOLL_CTL_DEL; // Overridden

      vLrtLlistFreeNode(px_full_ctx->x_ldb.px_llist_watches,
                        px_node_watch_this);
    }
      
    // Parameters have been recorded to flags, if valid
    if (i_epoll_op != LDB_EPOLL_OP_INVALID)
    {
      vLdbWriteControl(px_full_ctx,
                       LDB_CONTROL_MAIN_OP_WATCH,
                       i_epoll_op,
                       u32_epoll_event_flags,
                       dbus_watch_get_unix_fd(px_dbus_watch));

      return;
    }
  }

  return;
}


static void vLdbToggleWatch(DBusWatch* px_dbus_watch, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_node_watch* px_node_watch_this = NULL;

  printf("Watch toggle called\n");
  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  px_node_watch_this =
    (lrt_ldb_node_watch*)pxLrtLlistFindNode(px_full_ctx->x_ldb.px_llist_watches,
                                            LRT_LLIST_FIND_EQUAL,
                                            iCompareNodeAndDbusWatchFds,
                                            px_dbus_watch);

  if (px_node_watch_this != NULL)
  {
    if (dbus_watch_get_flags(px_dbus_watch) & DBUS_WATCH_READABLE)
    {
      if (dbus_watch_get_enabled(px_dbus_watch) == TRUE)
      {
        px_node_watch_this->u32_epoll_event_flags |= EPOLLIN;
      }
      else
      {
        px_node_watch_this->u32_epoll_event_flags &= ~EPOLLIN;
      }
    }
    else if (dbus_watch_get_flags(px_dbus_watch) & DBUS_WATCH_WRITABLE)
    {
      if (dbus_watch_get_enabled(px_dbus_watch) == TRUE)
      {
        px_node_watch_this->u32_epoll_event_flags |= EPOLLOUT;
      }
      else
      {
        px_node_watch_this->u32_epoll_event_flags &= ~EPOLLOUT;
      }
    }
    vLdbWriteControl(px_full_ctx,
                     LDB_CONTROL_MAIN_OP_WATCH,
                     EPOLL_CTL_MOD,
                     px_node_watch_this->u32_epoll_event_flags,
                     px_node_watch_this->i_watch_fd);

  }

  return;
}


static int iCompareNodeAndDbusTimeout(lrt_llist_node* px_list_node, void* pv_dbus_timeout_data)
{
  if (((DBusWatch*)(((lrt_ldb_node_timeout*)px_list_node)->px_dbus_timeout)) ==
      ((DBusWatch*)pv_dbus_timeout_data))
  {
    return LDB_COMPARE_NEUTRAL;
  }
  
  return LDB_COMPARE_NEGATIVE;
}


static dbus_bool_t tLdbAddTimeout(DBusTimeout* px_dbus_timeout, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_node_timeout* px_node_timeout_this = NULL;
  lrt_ldb_node_timeout* px_node_timeout_new = NULL;
  void* pv_malloc_test = NULL;

  printf("Timeout add called\n");
  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  px_node_timeout_this =
    (lrt_ldb_node_timeout*)pxLrtLlistFindNode(px_full_ctx->x_ldb.px_llist_timeouts,
                                              LRT_LLIST_FIND_EQUAL,
                                              iCompareNodeAndDbusTimeout,
                                              px_dbus_timeout);
  
  if (px_node_timeout_this == NULL)
  {
    // Need to add new.
    pv_malloc_test = malloc(sizeof(lrt_ldb_node_timeout));

    if (pv_malloc_test == NULL)
    {
      return FALSE;
    }
    px_node_timeout_new = (lrt_ldb_node_timeout*)(pv_malloc_test);
    px_node_timeout_new->px_dbus_timeout = px_dbus_timeout;
    vLrtLlistAddNode(px_full_ctx->x_ldb.px_llist_timeouts,
                     px_node_timeout_new);
    // Also for timeouts, set context data for easier retrieval
    dbus_timeout_set_data(px_dbus_timeout, px_full_ctx, NULL);
    // Assign to common
    px_node_timeout_this = px_node_timeout_new;
  }
  
  // px_event_timeout_iterator is now proper, we can do now stuff
  if (dbus_timeout_get_enabled(px_node_timeout_this->px_dbus_timeout) == TRUE)
  {
    px_node_timeout_this->u8_enabled = LDB_TRUE;
    px_node_timeout_this->i_timeout_left =
      dbus_timeout_get_interval(px_node_timeout_this->px_dbus_timeout);
  }
  else
  {
    px_node_timeout_this->u8_enabled = LDB_FALSE;
    px_node_timeout_this->i_timeout_left = LDB_TIMEOUT_INVALID;
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
  lrt_ldb_node_timeout* px_node_timeout_this = NULL;

  printf("Timeout remove called\n");
  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  px_node_timeout_this =
    (lrt_ldb_node_timeout*)pxLrtLlistFindNode(px_full_ctx->x_ldb.px_llist_timeouts,
                                              LRT_LLIST_FIND_EQUAL,
                                              iCompareNodeAndDbusTimeout,
                                              px_dbus_timeout);
  
  if (px_node_timeout_this != NULL)
  {
    vLrtLlistFreeNode(px_full_ctx->x_ldb.px_llist_timeouts,
                      px_node_timeout_this);
    vLdbWriteControl(px_full_ctx,
                     LDB_CONTROL_MAIN_OP_TIMEOUT,
                     LDB_EPOLL_OP_INVALID,
                     LDB_EPOLL_EVENT_FLAGS_NONE,
                     LDB_FD_INVALID);
  }
}



static void vLdbToggleTimeout(DBusTimeout* px_dbus_timeout, void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  lrt_ldb_node_timeout* px_node_timeout_this = NULL;

  printf("Timeout toggle called\n");
  px_full_ctx = (libruuvitag_context_type*)pv_arg_data;
  px_node_timeout_this =
    (lrt_ldb_node_timeout*)pxLrtLlistFindNode(px_full_ctx->x_ldb.px_llist_timeouts,
                                              LRT_LLIST_FIND_EQUAL,
                                              iCompareNodeAndDbusTimeout,
                                              px_dbus_timeout);

  if (px_node_timeout_this != NULL)
  {
    // Toggling if different
    if ((dbus_timeout_get_enabled(px_dbus_timeout) == TRUE) &&
        (dbus_timeout_get_enabled(px_node_timeout_this->px_dbus_timeout) == FALSE))
    {
      px_node_timeout_this->u8_enabled = LDB_TRUE;
      px_node_timeout_this->i_timeout_left =
        dbus_timeout_get_interval(px_node_timeout_this->px_dbus_timeout);
      vLdbWriteControl(px_full_ctx,
                       LDB_CONTROL_MAIN_OP_TIMEOUT,
                       LDB_EPOLL_OP_INVALID,
                       LDB_EPOLL_EVENT_FLAGS_NONE,
                       LDB_FD_INVALID);
    }
    else if ((dbus_timeout_get_enabled(px_dbus_timeout) == FALSE) &&
             (dbus_timeout_get_enabled(px_node_timeout_this->px_dbus_timeout) == TRUE))
    {
      px_node_timeout_this->u8_enabled = LDB_FALSE;
      px_node_timeout_this->i_timeout_left = LDB_TIMEOUT_INVALID;
      vLdbWriteControl(px_full_ctx,
                       LDB_CONTROL_MAIN_OP_TIMEOUT,
                       LDB_EPOLL_OP_INVALID,
                       LDB_EPOLL_EVENT_FLAGS_NONE,
                       LDB_FD_INVALID);
    }
  }
}



static void vLdbDeinitEventLoopWithDbus(libruuvitag_context_type* px_full_ctx)
{
  struct epoll_event x_epoll_temp_event;
  
  // Need to work backwards on the resources

  // Coreloop
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_CORELOOP_RUNNING)
  {
    // DOes nothing, but for sanity.
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_CORELOOP_RUNNING;
  }
  // Pending calls

  // Timeouts
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_TIMEOUTS_ADDED)
  {
    dbus_connection_set_timeout_functions(px_full_ctx->x_ldb.px_dbus_conn,
					  NULL, NULL, NULL, NULL, NULL);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_TIMEOUTS_ADDED;
  }

  // Watches
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_WATCHES_ADDED)
  {
    dbus_connection_set_watch_functions(px_full_ctx->x_ldb.px_dbus_conn,
					  NULL, NULL, NULL, NULL, NULL);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_WATCHES_ADDED;
  }

  // Interfaces removed subscription
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_IFACES_REMOVED)
  {
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
    dbus_connection_flush(px_full_ctx->x_ldb.px_dbus_conn);
    dbus_connection_unref(px_full_ctx->x_ldb.px_dbus_conn);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_CONN;
  }

  // Remove control pipe from epoll
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_EPOLL_CTRL_PIPE_ADDED)
  {
    x_epoll_temp_event.events = 0;
    x_epoll_temp_event.data.fd = px_full_ctx->x_ldb.i_evl_control_read_fd;
    epoll_ctl(px_full_ctx->x_ldb.i_epoll_fd, EPOLL_CTL_DEL,
              x_epoll_temp_event.data.fd, &x_epoll_temp_event);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_EPOLL_CTRL_PIPE_ADDED;
  }

  // Remove the epoll structure itself
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_EPOLL_CREATED)
  {
    close(px_full_ctx->x_ldb.i_epoll_fd);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_EPOLL_CREATED;
  }
  
  // Control pipe
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_CTRL_PIPE)
  {
    close(px_full_ctx->x_ldb.i_evl_control_write_fd);
    close(px_full_ctx->x_ldb.i_evl_control_read_fd);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_CTRL_PIPE;
  }

  // Linked lists
  if (px_full_ctx->x_ldb.u32_inited_flags & LDB_INITED_FLAGS_LINKED_LISTS)
  {
    vLrtLlistFreeAll(px_full_ctx->x_ldb.px_llist_pending_calls);
    vLrtLlistFreeAll(px_full_ctx->x_ldb.px_llist_timeouts);
    vLrtLlistFreeAll(px_full_ctx->x_ldb.px_llist_watches);
    px_full_ctx->x_ldb.u32_inited_flags &= ~LDB_INITED_FLAGS_LINKED_LISTS;
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


static int iDispatchDbusWatchFromEpollEvent(lrt_llist_node* px_list_node, void* pv_epoll_event_data)
{
  lrt_ldb_node_watch* px_node_watch = NULL;
  struct epoll_event* px_epoll_event = NULL;

  px_node_watch = (lrt_ldb_node_watch*)px_list_node;
  px_epoll_event = (struct epoll_event*)pv_epoll_event_data;

  if (px_node_watch->i_watch_fd == px_epoll_event->data.fd)
  {
    if ((px_epoll_event->events & EPOLLIN) & (px_node_watch->u32_epoll_event_flags))
    {
      printf("HANDLING READ\n");
      dbus_watch_handle(px_node_watch->px_dbus_read_watch, DBUS_WATCH_READABLE);
    }
    if ((px_epoll_event->events & EPOLLOUT) & (px_node_watch->u32_epoll_event_flags))
    {
      printf("Handling WRITE\n");
      dbus_watch_handle(px_node_watch->px_dbus_write_watch, DBUS_WATCH_WRITABLE);
    }
  }
  
  return LDB_SUCCESS;
}





static uint8_t u8EpollWaitAndDispatch(libruuvitag_context_type* px_full_ctx)
{
  static struct epoll_event ax_epoll_monitor_events[LDB_EPOLL_MONITOR_NUM_EVENTS];
  struct epoll_event x_epoll_temp_event;
  struct timespec x_wait_start;
  struct timespec x_wait_end;
  int i_epoll_fds_ready = 0;
  int i_next_timeout = LDB_TIMEOUT_INVALID;
  int i_epoll_iterator = 0;
  
  uint8_t u8_main_op = LDB_CONTROL_NO_OP;
  int i_epoll_op = LDB_EPOLL_OP_INVALID;
  uint32_t u32_epoll_event_flags = LDB_EPOLL_EVENT_FLAGS_NONE;
  int i_control_passed_fd = LDB_FD_INVALID;

  
  //clock_gettime(CLOCK_MONOTONIC, 
  i_epoll_fds_ready = epoll_wait(px_full_ctx->x_ldb.i_epoll_fd, ax_epoll_monitor_events,
                                 LDB_EPOLL_MONITOR_NUM_EVENTS, i_next_timeout);



  for (i_epoll_iterator = 0; i_epoll_iterator < i_epoll_fds_ready; i_epoll_iterator++)
  {
    if (ax_epoll_monitor_events[i_epoll_iterator].data.fd ==
        px_full_ctx->x_ldb.i_evl_control_read_fd)
    {
      if (u8LdbReadControl(px_full_ctx, &u8_main_op, &i_epoll_op,
                           &u32_epoll_event_flags, &i_control_passed_fd) == LDB_SUCCESS)
      {
        if (u8_main_op == LDB_CONTROL_MAIN_OP_TERMINATE)
        {
          printf("Terminating as requested\n");

          return LDB_FAIL;
        }
        else if (u8_main_op == LDB_CONTROL_MAIN_OP_RECEIVERS)
        {
          printf("Scanning for receivers\n");
        }
        else if (u8_main_op == LDB_CONTROL_MAIN_OP_WATCH)
        {
          /*
          printf("u8_main_op=%u  i_epoll_op=%d  u32_epoll_event_flags=%u  i_control_passed_fd=%d\n",
                 u8_main_op, i_epoll_op, u32_epoll_event_flags, i_control_passed_fd);
          */
          x_epoll_temp_event.events = u32_epoll_event_flags;
          x_epoll_temp_event.data.ptr = NULL;
          x_epoll_temp_event.data.fd = i_control_passed_fd;

          if (epoll_ctl(px_full_ctx->x_ldb.i_epoll_fd, i_epoll_op,
                        x_epoll_temp_event.data.fd, &x_epoll_temp_event) == LDB_EPOLL_OP_INVALID)
          {
            return LDB_FAIL;
          }
        }
        else if (u8_main_op == LDB_CONTROL_MAIN_OP_TIMEOUT)
        {
          // Handled more or lessa utomatically
        }
      }
    }
    else
    {
      vLrtLlistApplyFunc(px_full_ctx->x_ldb.px_llist_watches,
                         iDispatchDbusWatchFromEpollEvent,
                         &(ax_epoll_monitor_events[i_epoll_iterator]));
    }
  }
  return LDB_SUCCESS;
}




static void* vLdbEventLoopBody(void* pv_arg_data)
{
  libruuvitag_context_type* px_full_ctx = NULL;
  int ai_pipe_fds[2];
  // If I read docs correctly, exception fds are not needed.
  // Strange that wpa_supplicant uses them. I am probably reading wrong.
  uint8_t u8_evl_running = LDB_SUCCESS;

  struct epoll_event x_epoll_temp_event;

  
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
  x_epoll_temp_event.data.ptr = NULL;
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
  
  while (u8_evl_running == LDB_SUCCESS)
  {
    //sleep(1); // just in case for now

    if (u8EpollWaitAndDispatch(px_full_ctx) == LDB_FAIL)
    {
      u8_evl_running = LDB_FAIL;
    }
  }
  // Out of event loop, most probably because we are deinitializing.
  // Release resources.
  vLdbDeinitEventLoopWithDbus(px_full_ctx);

  return NULL;
}


static uint8_t u8LdbInitLocalContext(libruuvitag_context_type* px_full_ctx)
{
  px_full_ctx->x_ldb.u32_inited_flags = LDB_INITED_FLAGS_NONE;
  px_full_ctx->x_ldb.i_evl_control_write_fd = LDB_FD_INVALID;
  px_full_ctx->x_ldb.i_evl_control_read_fd = LDB_FD_INVALID;
  px_full_ctx->x_ldb.i_epoll_fd = LDB_FD_INVALID;
  px_full_ctx->x_ldb.px_llist_watches = pxLrtLlistNew();
  px_full_ctx->x_ldb.px_llist_timeouts = pxLrtLlistNew();
  px_full_ctx->x_ldb.px_llist_pending_calls = pxLrtLlistNew();
  px_full_ctx->x_ldb.u32_inited_flags |= LDB_INITED_FLAGS_LINKED_LISTS;

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
  sleep(1); // Sleep before asking receivers rehash
  vLdbWriteControl(px_full_ctx,
                   LDB_CONTROL_MAIN_OP_RECEIVERS,
                   LDB_EPOLL_OP_INVALID,
                   LDB_EPOLL_EVENT_FLAGS_NONE,
                   LDB_FD_INVALID);
  
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

