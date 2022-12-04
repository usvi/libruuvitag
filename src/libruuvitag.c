
#include "libruuvitag.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>

#include <gio/gio.h>

// BUF sizes including trailing null
#define LRT_MAC_BUF_SIZE (18)
#define LRT_BLUEZ_DEV_OBJECT_PATH  (128) /* Something like /org/bluez/hci0 */

typedef struct t_bluez_pair_mac_list_node t_bluez_pair_mac_list_node;

struct t_bluez_pair_mac_list_node
{
  char s_mac_buffer_uppercase[LRT_MAC_BUF_SIZE];
  char s_bluez_dev_object_path[LRT_BLUEZ_DEV_OBJECT_PATH];
  t_bluez_pair_mac_list_node* next;
};


//GDBusConnection* gpx_dbus_connection;


uint8_t u8LibRuuvitagInit(void)
{
  printf("Libruuvitag starting\n");

  
  
  return 0;
}
