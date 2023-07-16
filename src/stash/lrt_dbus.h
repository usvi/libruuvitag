#ifndef _LRT_DBUS_H_
#define _LRT_DBUS_H_

#include <stdint.h>
#include <semaphore.h>

#include <dbus/dbus.h>


typedef struct lrt_dbus_type lrt_dbus_type;
typedef struct lrt_context_type lrt_context_type;


struct lrt_dbus_type
{
  uint8_t u8_event_loop_running;
  pthread_t x_event_loop_thread;
  DBusConnection* px_sys_conn;

};


struct lrt_context_type
{
  sem_t x_shared_data_semaphore;
  lrt_dbus_type* px_dbus;
};



uint8_t u8LrtInitDbus(lrt_context_type* px_lrt_context);
void vLrtDeinitDbus(lrt_context_type* px_lrt_context);

#endif // #ifndef _LRT_DBUS_H_
