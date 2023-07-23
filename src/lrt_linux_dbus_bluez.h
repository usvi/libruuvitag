#ifndef _LRT_LINUX_DBUS_BLUEZ_H_
#define _LRT_LINUX_DBUS_BLUEZ_H_


// Headers needed in crafting our own dbus struct type.
#include <inttypes.h>
#include <semaphore.h>
#include <pthread.h>
#include <dbus/dbus.h>


// Careful definition ordering to keep top-level header
// happy and at the same time define strictly the dbus-
// related stuff here.
typedef struct lrt_ldb_context_type lrt_ldb_context_type;
typedef struct lrt_ldb_watch lrt_ldb_watch;
typedef struct lrt_ldb_timeout lrt_ldb_timeout;

struct lrt_ldb_watch
{
  DBusWatchFlags e_watch_type;
  dbus_bool_t t_enabled;
  int i_watch_fd;
  DBusWatch* px_dbus_watch;
  lrt_ldb_watch* px_next_watch;
};

struct lrt_ldb_timeout
{
  dbus_bool_t t_enabled;
  struct timeval x_interval;
  struct timeval x_next_deadline;
  DBusTimeout* px_dbus_timeout;
  lrt_ldb_timeout* px_next_timeout;
};

struct lrt_ldb_context_type
{
  DBusConnection* px_dbus_conn;
  uint8_t u8_running;

  int i_evl_control_write_fd;
  int i_evl_control_read_fd;
  sem_t x_evl_sem;
  pthread_t x_evl_thread;

  lrt_ldb_watch* px_event_watches;
  lrt_ldb_timeout* px_event_timeouts;
};


// Top-level header
#include "libruuvitag.h"


// Normal defines
#define LDB_TRUE           (1)
#define LDB_FALSE          (0)

#define LDB_SUCCESS        (1)
#define LDB_FAIL           (0)
#define LDB_AGAIN          (2)


#define LDB_CONTROL_ERROR             (0)
#define LDB_CONTROL_TERMINATE         (1)
#define LDB_CONTROL_DBUS_WATCHES      (2)


// Function prototypes
uint8_t u8LrtInitLinuxDbusBluez(libruuvitag_context_type* px_full_context);
uint8_t u8LrtDeinitLinuxDbusBluez(libruuvitag_context_type* px_full_context);

#endif // #ifndef _LRT_LINUX_DBUS_BLUEZ_H_

