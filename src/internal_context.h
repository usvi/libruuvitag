#ifndef _LIBRUUVITAG_INTERNAL_CONTEXT_H_
#define _LIBRUUVITAG_INTERNAL_CONTEXT_H_

#include <semaphore.h>

#include <dbus/dbus.h>


typedef struct
{
  DBusConnection* px_dbus_sys_conn;
  sem_t x_shared_data_semaphore;
  
} x_libruuvitag_context_type;

#endif // #ifndef _LIBRUUVITAG_INTERNAL_CONTEXT_H_
