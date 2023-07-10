#include "libruuvitag.h"

#ifdef BACKEND_LINUX_DBUS_BLUEZ
#include "lrt_linux_dbus_bluez.h"
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>



libruuvitag_context_type* pxLibRuuviTagInit(char* s_listen_on_bt_adapters, char* s_listen_to_ruuvitags)
{
  void* pv_malloc = NULL;
  libruuvitag_context_type* px_created_context;
  uint8_t u8_backend_init_res;

  pv_malloc = malloc(sizeof(libruuvitag_context_type));

  if (pv_malloc == NULL)
  {
    return NULL;
  }

  px_created_context = pv_malloc;

#ifdef BACKEND_LINUX_DBUS_BLUEZ
  u8_backend_init_res = u8LrtInitLinuxDbusBluez(px_created_context);
  if (u8_backend_init_res != LDB_SUCCESS)
#endif // #ifdef BACKEND_LINUX_DBUS_BLUEZ
  {
    free(px_created_context);

    return NULL;
  }
  
  return px_created_context;
}

void vLibRuuviTagDeinit(libruuvitag_context_type* px_context)
{
  if (px_context != NULL)
  {
#ifdef BACKEND_LINUX_DBUS_BLUEZ
    u8LrtDeinitLinuxDbusBluez(px_context);
#endif // #ifdef BACKEND_LINUX_DBUS_BLUEZ
    
    free(px_context);
  }
}
