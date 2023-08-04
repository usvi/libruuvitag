#ifndef _LRT_LINUX_DBUS_BLUEZ_H_
#define _LRT_LINUX_DBUS_BLUEZ_H_


// Headers needed in crafting our own dbus struct type.
#include "lrt_llist.h"

#include <inttypes.h>
#include <semaphore.h>
#include <pthread.h>

#include <dbus/dbus.h>


// Careful definition ordering to keep top-level header
// happy and at the same time define strictly the dbus-
// related stuff here.
typedef struct lrt_ldb_context_type lrt_ldb_context_type;
typedef struct lrt_ldb_node_watch lrt_ldb_node_watch;
typedef struct lrt_ldb_node_timeout lrt_ldb_node_timeout;


struct lrt_ldb_node_watch
{
  lrt_llist_node* px_prev_node;
  lrt_llist_node* px_next_node;
  
  int i_watch_fd;
  uint32_t u32_epoll_event_flags;
  DBusWatch* px_dbus_read_watch;
  DBusWatch* px_dbus_write_watch;
};

struct lrt_ldb_node_timeout
{
  lrt_llist_node* px_prev_node;
  lrt_llist_node* px_next_node;
  
  uint8_t u8_enabled;
  int i_timeout_left;
  DBusTimeout* px_dbus_timeout;
};

struct lrt_ldb_context_type
{
  DBusConnection* px_dbus_conn;
  volatile uint32_t u32_inited_flags;
  sem_t x_inited_sem;

  int i_evl_control_write_fd;
  int i_evl_control_read_fd;
  int i_epoll_fd;
  pthread_t x_evl_thread;

  lrt_llist_head* px_llist_watches;
  lrt_llist_head* px_llist_timeouts;
  
  /*
  lrt_ldb_watch* px_event_watches;
  lrt_ldb_timeout* px_event_timeouts;
  */
};


// Top-level header
#include "libruuvitag.h"


// Normal defines

#define LDB_SUCCESS        (1)
#define LDB_FAIL           (0)
#define LDB_AGAIN          (2)



// Function prototypes
uint8_t u8LrtInitLinuxDbusBluez(libruuvitag_context_type* px_full_context);
uint8_t u8LrtDeinitLinuxDbusBluez(libruuvitag_context_type* px_full_context);

#endif // #ifndef _LRT_LINUX_DBUS_BLUEZ_H_

