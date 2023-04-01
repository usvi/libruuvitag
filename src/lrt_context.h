#ifndef _LRT_CONTEXT_H_
#define _LRT_CONTEXT_H_

#include <stdint.h>
#include <semaphore.h>
#include <dbus/dbus.h>


typedef struct
{
  uint8_t u8_event_loop_running;
  pthread_t x_event_loop_thread;
  DBusConnection* px_sys_conn;

} lrt_dbus_type;


typedef struct
{
  sem_t x_shared_data_semaphore;
  lrt_dbus_type* px_dbus;
} lrt_context_type;


#endif // #ifndef _LRT_CONTEXT_H_
