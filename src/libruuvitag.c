
#include "libruuvitag.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <semaphore.h>

#include <glib.h>
#include <gio/gio.h>

static sem_t gx_glib_main_loop_semaphore;
static pthread_t gt_glib_main_loop_thread;
static void* pvGlibMainLoopThreadBody(void* pv_params);

static GDBusConnection* gpx_dbus_connection = NULL;
static GMainLoop* gpx_glib_main_loop;
static guint gt_interfaces_added_handle = 0;


// Internals: 
typedef struct t_bluez_pair_mac_list_node t_bluez_pair_mac_list_node;

struct t_bluez_pair_mac_list_node
{
  char s_mac_buffer_uppercase[LIBRUUVITAG_MAC_BUF_SIZE];
  char s_bluez_dev_object_path[LIBRUUVITAG_BLUEZ_DEV_OBJECT_PATH];
  t_bluez_pair_mac_list_node* next;
};




static void vLRTInterfaceAddedCallback(GDBusConnection *sig,
                                       const gchar *sender_name,
                                       const gchar *object_path,
                                       const gchar *interface,
                                       const gchar *signal_name,
                                       GVariant *parameters,
                                       gpointer user_data)
{

  printf("Interface added!\n");
}


void* pvGlibMainLoopThreadBody(void* pv_params)
{
  sem_wait(&gx_glib_main_loop_semaphore);
  g_main_loop_run(gpx_glib_main_loop);
  
  return NULL;
}


uint8_t u8LibRuuvitagInit(char* s_listen_on)
{
  printf("Libruuvitag starting\n");
  sem_init(&gx_glib_main_loop_semaphore, 0, 0);

  GError *error = NULL;

  gpx_dbus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);

  gpx_glib_main_loop = g_main_loop_new(NULL, FALSE);
  
  gt_interfaces_added_handle = g_dbus_connection_signal_subscribe(gpx_dbus_connection,
                                                                  "org.bluez",
                                                                  "org.freedesktop.DBus.ObjectManager",
                                                                  "InterfacesAdded",
                                                                  NULL,
                                                                  NULL,
                                                                  G_DBUS_SIGNAL_FLAGS_NONE,
                                                                  vLRTInterfaceAddedCallback,
                                                                  NULL,
                                                                  NULL);

  //g_main_loop_run(loop);
  pthread_create(&gt_glib_main_loop_thread, NULL, pvGlibMainLoopThreadBody, NULL);
  sem_post(&gx_glib_main_loop_semaphore);
  
  return 0;
}


uint8_t u8LibRuuvitagDeinit(void)
{
  printf("Libruuvitag stopping\n");
  g_dbus_connection_signal_unsubscribe(gpx_dbus_connection, gt_interfaces_added_handle);
  g_main_loop_quit(gpx_glib_main_loop);
  g_object_unref(gpx_dbus_connection);
  pthread_join(gt_glib_main_loop_thread, NULL);

  return 0;
}
