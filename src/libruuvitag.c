
#include "libruuvitag.h"

#include "lrt_dbus.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>



libruuvitag_context_type* pxLibRuuviTagInit(char* s_listen_on_bt_adapters, char* s_listen_to_ruuvitags)
{
  void* pv_malloc = NULL;
  libruuvitag_context_type* px_created_context;

  pv_malloc = malloc(sizeof(libruuvitag_context_type));

  if (pv_malloc == NULL)
  {
    return NULL;
  }

  px_created_context = pv_malloc;

  return px_created_context;
}

void vLibRuuviTagDeinit(libruuvitag_context_type* px_context)
{
  if (px_context != NULL)
  {
    // Do first other teardown, then this
    free(px_context);
  }
}
