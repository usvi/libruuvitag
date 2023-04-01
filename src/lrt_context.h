#ifndef _LRT_CONTEXT_H_
#define _LRT_CONTEXT_H_

#include <semaphore.h>
#include <dbus/dbus.h>

typedef struct lrt_dbus_type lrt_dbus_type;
typedef struct lrt_context_type lrt_context_type;


struct lrt_dbus_type
{
  DBusConnection* px_sys_conn;

};


struct lrt_context_type
{
  sem_t x_shared_data_semaphore;
  lrt_dbus_type* px_dbus;
};


#endif // #ifndef _LRT_CONTEXT_H_
