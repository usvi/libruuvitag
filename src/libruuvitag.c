
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
static GMainLoop* gpx_glib_main_loop = NULL;
static guint gt_interfaces_changed_subs_id = 0;
static GError* gpx_glib_error = NULL;



// Internals: 
typedef struct t_bluez_pair_mac_list_node t_bluez_pair_mac_list_node;

struct t_bluez_pair_mac_list_node
{
  char s_mac_buffer_uppercase[LIBRUUVITAG_MAC_BUF_SIZE];
  char s_bluez_dev_object_path[LIBRUUVITAG_BLUEZ_DEV_OBJECT_PATH];
  t_bluez_pair_mac_list_node* next;
};


static void vReconfigureReceivers(void)
{
  GDBusProxy* px_glib_dbus_proxy = NULL;
  GError* px_glib_error = NULL;
  GVariant* px_glib_top_container_variant = NULL;
  GVariant* px_glib_top_content_variant = NULL;
  GVariantIter x_glib_top_content_iterator;
  gchar* t_glib_object_path = NULL;
  GVariant* px_glib_interface_and_properties_variant = NULL;
  GVariantIter x_glib_interface_and_properties_iterator;
  gchar* t_glib_interface_name = NULL;
  GVariant* px_glib_properties_variant = NULL;
  GVariantIter x_glib_adapter_properties_iterator;
  gchar* t_glib_adapter_property_key = NULL;
  GVariant* px_glib_adapter_property_variant = NULL;
  const gchar* t_glib_adapter_address = NULL;
  
  px_glib_dbus_proxy =
    g_dbus_proxy_new_sync(gpx_dbus_connection,
                          G_DBUS_PROXY_FLAGS_NONE,
                          NULL,
                          "org.bluez",
                          "/",
                          "org.freedesktop.DBus.ObjectManager",
                          NULL,
                          &px_glib_error);

  if (px_glib_error != NULL)
  {
    return;
  }

  px_glib_top_container_variant =
    g_dbus_proxy_call_sync(px_glib_dbus_proxy,
                           "GetManagedObjects",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           &px_glib_error);

  if (px_glib_error != NULL)
  {
    return;
  }

  if (px_glib_top_container_variant)
  {
    px_glib_top_content_variant = g_variant_get_child_value(px_glib_top_container_variant, 0);
    g_variant_iter_init(&x_glib_top_content_iterator,
                        px_glib_top_content_variant);

    while (g_variant_iter_next(&x_glib_top_content_iterator,
                               "{&o@a{sa{sv}}}",
                               &t_glib_object_path,
                               &px_glib_interface_and_properties_variant))
    {
      // We have array if dicts. Now we need a dict with interface value org.bluez.Adapter1 .
      // We can then scan properties of that variant
      g_variant_iter_init(&x_glib_interface_and_properties_iterator,
                          px_glib_interface_and_properties_variant);

      while (g_variant_iter_next(&x_glib_interface_and_properties_iterator,
                                 "{&s@a{sv}}",
                                 &t_glib_interface_name,
                                 &px_glib_properties_variant))
      {
        if (g_str_equal(t_glib_interface_name, "org.bluez.Adapter1"))
        {

          // And finally, we need to go trough individual datas
          g_variant_iter_init(&x_glib_adapter_properties_iterator,
                              px_glib_properties_variant);

          while (g_variant_iter_next(&x_glib_adapter_properties_iterator,
                                     "{&sv}",
                                     &t_glib_adapter_property_key,
                                     &px_glib_adapter_property_variant))
          {
            if (g_str_equal(t_glib_adapter_property_key, "Address"))
            {
              t_glib_adapter_address =
                g_variant_get_string(px_glib_adapter_property_variant, NULL);
              g_print("%s\n", t_glib_adapter_address);
              // No need to free the string as variant free does it
            }
            g_variant_unref(px_glib_adapter_property_variant);
            g_free(t_glib_adapter_property_key);
          }
        }
        g_variant_unref(px_glib_properties_variant);
        g_free(t_glib_interface_name);
      }
      g_variant_unref(px_glib_interface_and_properties_variant);
      g_free(t_glib_object_path);
    }
                               
  }
}




static void vInterfaceChangedCallback(GDBusConnection *sig,
                                       const gchar *sender_name,
                                       const gchar *object_path,
                                       const gchar *interface,
                                       const gchar *signal_name,
                                       GVariant *parameters,
                                       gpointer user_data)
{
  printf("Interface changed!\n");
  vReconfigureReceivers();
}


void* pvGlibMainLoopThreadBody(void* pv_params)
{
  sem_wait(&gx_glib_main_loop_semaphore);
  g_main_loop_run(gpx_glib_main_loop);
  
  return NULL;
}


uint8_t u8LibRuuviTagInit(char* s_listen_on)
{
  printf("Libruuvitag starting\n");
  sem_init(&gx_glib_main_loop_semaphore, 0, 0);


  gpx_dbus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &gpx_glib_error);

  gpx_glib_main_loop = g_main_loop_new(NULL, FALSE);
  
  gt_interfaces_changed_subs_id =
    g_dbus_connection_signal_subscribe(gpx_dbus_connection,
                                       "org.bluez",
                                       "org.freedesktop.DBus.ObjectManager",
                                       "InterfacesAdded",
                                       NULL,
                                       NULL,
                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                       vInterfaceChangedCallback,
                                       NULL,
                                       NULL);

  //g_main_loop_run(loop);
  pthread_create(&gt_glib_main_loop_thread, NULL, pvGlibMainLoopThreadBody, NULL);
  sem_post(&gx_glib_main_loop_semaphore);
  vReconfigureReceivers();
  
  return 0;
}


uint8_t u8LibRuuviTagDeinit(void)
{
  printf("Libruuvitag stopping\n");
  g_dbus_connection_signal_unsubscribe(gpx_dbus_connection, gt_interfaces_changed_subs_id);
  g_main_loop_quit(gpx_glib_main_loop);
  pthread_join(gt_glib_main_loop_thread, NULL);
  g_object_unref(gpx_dbus_connection);

  if (gpx_glib_error != NULL)
  {
    g_error_free(gpx_glib_error);
  }

  return 0;
}
