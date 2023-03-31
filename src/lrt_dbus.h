#ifndef _LRT_GLUE_DBUS_H_
#define _LRT_GLUE_DBUS_H_

#include "lrt_context.h"

#include <stdint.h>

#include <dbus/dbus.h>

uint8_t u8LrtInitDbus(lrt_context_type* px_lrt_context);
void vLrtDeinitDbus(lrt_context_type* px_lrt_context);

#endif // #ifndef _LRT_GLUE_DBUS_H_
