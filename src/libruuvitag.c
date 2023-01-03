
#include "libruuvitag.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <semaphore.h>

#include <glib.h>
#include <gio/gio.h>


volatile uint8_t gu8_dbus_semaphores_inited = 0;
static sem_t gx_mutation_semaphore;

uint8_t gu8_glib_main_loop_thread_created = 0;
static sem_t gx_glib_main_loop_semaphore;
static pthread_t gt_glib_main_loop_thread;
static void* pvGlibMainLoopThreadBody(void* pv_params);

static GDBusConnection* gpx_dbus_system_connection = NULL;
static GMainLoop* gpx_glib_main_loop = NULL;

// vSubscribeIfaceAddRemovals
uint8_t gu8_ifaces_add_removal_subscribed = 0;
static guint gt_interfaces_added_subs_id = 0;
static guint gt_interfaces_removed_subs_id = 0;



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
    g_dbus_proxy_new_sync(gpx_dbus_system_connection,
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




static void vSignalCallbackDirector(GDBusConnection *sig,
                                    const gchar *sender_name,
                                    const gchar *object_path,
                                    const gchar *interface,
                                    const gchar *signal_name,
                                    GVariant *parameters,
                                    gpointer user_data)
{
  /* We get like:

     Object path: /
     Interface: org.freedesktop.DBus.ObjectManager
     Signal name: InterfacesAdded

     OR
     
     Object path: /org/bluez/hci0/dev_CD_99_36_8E_6E_70
     Interface: org.freedesktop.DBus.Properties
     Signal name: PropertiesChanged
  */
  if ((g_strcmp0(object_path, "/") == 0) &&
      (g_strcmp0(interface, "org.freedesktop.DBus.ObjectManager") == 0) &&
      ((g_strcmp0(signal_name, "InterfacesAdded") == 0) ||
       (g_strcmp0(signal_name, "InterfacesRemoved") == 0)))
  {
    vReconfigureReceivers();
  }
}


void* pvGlibMainLoopThreadBody(void* pv_params)
{
  sem_wait(&gx_glib_main_loop_semaphore);
  g_main_loop_run(gpx_glib_main_loop);
  
  return NULL;
}

static void vInitSemaphores(void)
{
  if (!gu8_dbus_semaphores_inited)
  {
    sem_init(&gx_mutation_semaphore, 0, 1);
    sem_init(&gx_glib_main_loop_semaphore, 0, 0);
    gu8_dbus_semaphores_inited = 1;
  }
}


static uint8_t u8InitSystemDbusConnection(void)
{
  GError* px_glib_error = NULL;

  sem_wait(&gx_mutation_semaphore);

  if (!gpx_dbus_system_connection)
  {
    gpx_dbus_system_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &px_glib_error);

    if (px_glib_error)
    {
      g_free(px_glib_error);
      
      if (gpx_dbus_system_connection)
      {
        g_object_unref(gpx_dbus_system_connection);
      }
      sem_post(&gx_mutation_semaphore);
      
      return LIBRUUVITAG_RES_FATAL;
      }
  }
  sem_post(&gx_mutation_semaphore);

  return LIBRUUVITAG_RES_OK;
}

#define ASSERT_RESULT(ARG_GOT_RESULT, ARG_COMPARING_TO_RESULT) \
if (ARG_GOT_RESULT != ARG_COMPARING_TO_RESULT)                                \
{                                                                             \
  return ARG_GOT_RESULT;                                                      \
}


static void vInitMainLoop(void)
{
  sem_wait(&gx_mutation_semaphore);

  if (!gpx_glib_main_loop)
  {
    // Docs say this is never NULL after call:
    gpx_glib_main_loop = g_main_loop_new(NULL, FALSE);
  }      
  sem_post(&gx_mutation_semaphore);
}


static void vSubscribeIfaceAddRemovals(void)
{
  sem_wait(&gx_mutation_semaphore);
  if (!gu8_ifaces_add_removal_subscribed)
  {
    gt_interfaces_added_subs_id =
      g_dbus_connection_signal_subscribe(gpx_dbus_system_connection,
                                         "org.bluez",
                                         "org.freedesktop.DBus.ObjectManager",
                                         "InterfacesAdded",
                                         NULL,
                                         NULL,
                                         G_DBUS_SIGNAL_FLAGS_NONE,
                                         vSignalCallbackDirector,
                                         NULL,
                                         NULL);

    gt_interfaces_removed_subs_id =
      g_dbus_connection_signal_subscribe(gpx_dbus_system_connection,
                                         "org.bluez",
                                         "org.freedesktop.DBus.ObjectManager",
                                         "InterfacesRemoved",
                                         NULL,
                                         NULL,
                                         G_DBUS_SIGNAL_FLAGS_NONE,
                                         vSignalCallbackDirector,
                                         NULL,
                                         NULL);

    gu8_ifaces_add_removal_subscribed = 1;
  }
  sem_post(&gx_mutation_semaphore);
}

static void vCreateGlibMainLoopThread(void)
{
  sem_wait(&gx_mutation_semaphore);
  if (!gu8_glib_main_loop_thread_created)
  {
    
    pthread_create(&gt_glib_main_loop_thread, NULL, pvGlibMainLoopThreadBody, NULL);
    sem_post(&gx_glib_main_loop_semaphore);
  }
  sem_post(&gx_mutation_semaphore);
}


uint8_t u8LibRuuviTagInit(char* s_listen_on, char* s_listen_to)
{
  uint8_t u8_result = 0;
  
  vInitSemaphores();
  u8_result = u8InitSystemDbusConnection();
  ASSERT_RESULT(u8_result, LIBRUUVITAG_RES_OK);
  vInitMainLoop();
  vSubscribeIfaceAddRemovals();
  vCreateGlibMainLoopThread();
  vReconfigureReceivers();
  
  return LIBRUUVITAG_RES_OK;
}


uint8_t u8LibRuuviTagDeinit(void)
{
  printf("Libruuvitag stopping\n");
  g_dbus_connection_signal_unsubscribe(gpx_dbus_system_connection, gt_interfaces_removed_subs_id);
  g_dbus_connection_signal_unsubscribe(gpx_dbus_system_connection, gt_interfaces_added_subs_id);
  g_main_loop_quit(gpx_glib_main_loop);
  pthread_join(gt_glib_main_loop_thread, NULL);
  g_dbus_connection_close_sync(gpx_dbus_system_connection, NULL, NULL);
  g_object_unref(gpx_dbus_system_connection);

  return 0;
}
