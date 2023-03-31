
#include "libruuvitag.h"

#include "lrt_context.h"
#include "lrt_dbus.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>


static lrt_context_type gx_lib_context;


static uint8_t u8InitAllInContext(lrt_context_type* px_context)
{
  uint8_t u8_retval;
    
  u8_retval = u8LrtInitDbus(px_context);

  if (u8_retval != LIBRUUVITAG_RES_OK)
  {
    return u8_retval;
  }

  return u8_retval;
}

static void vDeinitAllInContext(lrt_context_type* px_context)
{
  vLrtDeinitDbus(px_context);
}

uint8_t u8LibRuuviTagInit(char* s_listen_on, char* s_listen_to)
{
  uint8_t u8_retval;

  printf("Libruuvitag starting\n");
  memset(&gx_lib_context, 0, sizeof(gx_lib_context));
  u8_retval = u8InitAllInContext(&gx_lib_context);

  return u8_retval;
}


uint8_t u8LibRuuviTagDeinit(void)
{
  printf("Libruuvitag stopping\n");
  vDeinitAllInContext(&gx_lib_context);

  return LIBRUUVITAG_RES_OK;
}
