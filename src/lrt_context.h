#ifndef _LRT_CONTEXT_H_
#define _LRT_CONTEXT_H_

#include <semaphore.h>
#include <dbus/dbus.h>

typedef struct 
{
  DBusConnection* px_sys_conn;

} lrt_dbus_type;


typedef struct
{
  sem_t x_shared_data_semaphore;
  lrt_dbus_type x_dbus;
} lrt_context_type;


#endif // #ifndef _LRT_CONTEXT_H_
