
#include "libruuvitag.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#include <dbus/dbus.h>



volatile uint8_t gu8_dbus_semaphores_inited = 0;

static sem_t gx_shared_data_semaphore;
static sem_t gx_msg_processing_semaphore;

uint8_t gu8_dbus_main_loop_thread_created = 0;
static sem_t gx_glib_main_loop_semaphore;
static pthread_t gt_glib_main_loop_thread;


static DBusConnection* gpx_dbus_system_conn = NULL;

uint8_t gu8_ifaces_add_removal_subscribed = 0;



// Internals: 
typedef struct t_bluez_pair_mac_list_node t_bluez_pair_mac_list_node;

struct t_bluez_pair_mac_list_node
{
  char s_mac_buffer_uppercase[LIBRUUVITAG_MAC_BUF_SIZE];
  char s_bluez_dev_object_path[LIBRUUVITAG_BLUEZ_DEV_OBJECT_PATH];
  t_bluez_pair_mac_list_node* next;
};






static void vIncomingMessageDirector(DBusMessage* px_dbus_msg)
{
  printf("Actual message got\n");

  dbus_message_unref(px_dbus_msg);
}

void* pvGlibMainLoopThreadBody(void* pv_params)
{
  DBusMessage* px_dbus_msg = NULL;
  
  sem_wait(&gx_glib_main_loop_semaphore);
  
  while (dbus_connection_read_write(gpx_dbus_system_conn, -1))
  {
    printf("Inside while\n");

    if (dbus_connection_get_dispatch_status(gpx_dbus_system_conn) ==
        DBUS_DISPATCH_DATA_REMAINS)
    {
      // Might have message
      sem_wait(&gx_msg_processing_semaphore);
      px_dbus_msg = dbus_connection_pop_message(gpx_dbus_system_conn);

      if (px_dbus_msg != NULL)
      {
        vIncomingMessageDirector(px_dbus_msg);
      }
      sem_post(&gx_msg_processing_semaphore);
    }
  }
  printf("After while\n");
  
  return NULL;
}

static void vInitSemaphores(void)
{
  if (!gu8_dbus_semaphores_inited)
  {
    gu8_dbus_semaphores_inited = 1;
    sem_init(&gx_shared_data_semaphore, 0, 1);
    sem_init(&gx_glib_main_loop_semaphore, 0, 0);
    sem_init(&gx_msg_processing_semaphore, 0, 1);
  }
}


static uint8_t u8InitSystemDbusConnection(void)
{
  DBusError x_dbus_error;
  
  sem_wait(&gx_shared_data_semaphore);
                   
  if (!gpx_dbus_system_conn)
  {
    dbus_error_init(&x_dbus_error);
    gpx_dbus_system_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &x_dbus_error);

    if (dbus_error_is_set(&x_dbus_error))
    {
      return LIBRUUVITAG_RES_FATAL;
    }
  }
  sem_post(&gx_shared_data_semaphore);

  return LIBRUUVITAG_RES_OK;
}

#define ASSERT_RESULT(ARG_GOT_RESULT, ARG_COMPARING_TO_RESULT) \
if (ARG_GOT_RESULT != ARG_COMPARING_TO_RESULT)                                \
{                                                                             \
  return ARG_GOT_RESULT;                                                      \
}




static void vCreateDbusMainLoopThread(void)
{
  sem_wait(&gx_shared_data_semaphore);
  
  if (!gu8_dbus_main_loop_thread_created)
  {
    pthread_create(&gt_glib_main_loop_thread, NULL, pvGlibMainLoopThreadBody, NULL);
  }
  sem_post(&gx_shared_data_semaphore);
}

static void vSetDbusMainLoopThreadRunning(void)
{
  sem_post(&gx_glib_main_loop_semaphore);
}

static void vSubscribeBluezInterfaceMessages(void)
{
  dbus_bus_add_match(gpx_dbus_system_conn,
                     "type='signal',"
                     "sender='org.bluez',"
                     "interface='org.freedesktop.DBus.ObjectManager',"
                     "member=InterfacesAdded",
                     NULL);
  dbus_bus_add_match(gpx_dbus_system_conn,
                     "type='signal',"
                     "sender='org.bluez',"
                     "interface='org.freedesktop.DBus.ObjectManager',"
                     "member=InterfacesRemoved",
                     NULL);
  dbus_connection_flush(gpx_dbus_system_conn);
}


static void vExtractIteratorDataRec(DBusMessageIter* px_root_iterator,
                                    uint8_t u8_depth,
                                    int i_this_container_type,
                                    char* s_object_path,
                                    char* s_interface,
                                    char* s_key)
{
  DBusMessageIter x_sub_iterator;
  int i_dbus_type;
  char* s_parsed_string = NULL;
  char* s_parsed_object_path = NULL;
  char* s_parsed_interface = NULL;
  char* s_parsed_key = NULL;
  uint8_t u8_a_string_parsed = 0;
  
  do
  {
    i_dbus_type = dbus_message_iter_get_arg_type(px_root_iterator);

    if (dbus_type_is_container(i_dbus_type))
    {
      // printf("Is container %c\n", i_dbus_type);
      dbus_message_iter_recurse(px_root_iterator, &x_sub_iterator);
      vExtractIteratorDataRec(&x_sub_iterator,
                              u8_depth + 1,
                              i_dbus_type,
                              
                              ((s_parsed_object_path != NULL) ?
                               s_parsed_object_path :
                               s_object_path),

                              ((s_parsed_interface != NULL) ?
                               s_parsed_interface :
                               s_interface),

                              ((s_parsed_key != NULL) ?
                               s_parsed_key :
                               s_key)
                              );

    }
    else if (dbus_type_is_basic(i_dbus_type))
    {
      //printf("Is basic %c\n", i_dbus_type);

      if (i_dbus_type == DBUS_TYPE_STRING)
      {
        if (u8_a_string_parsed > 0)
        {
          dbus_message_iter_get_basic(px_root_iterator,
                                      &s_parsed_string);

          printf("%s / %s . %s -> %s\n", s_object_path, s_interface, s_key, s_parsed_string);
        }
        else // First element
        {
          if (u8_depth == 4)
          {
            dbus_message_iter_get_basic(px_root_iterator,
                                        &s_parsed_interface);
          }
          else if (i_this_container_type == DBUS_TYPE_DICT_ENTRY)
          {
            dbus_message_iter_get_basic(px_root_iterator,
                                        &s_parsed_key);
          }
          else
          {
            dbus_message_iter_get_basic(px_root_iterator,
                                        &s_parsed_string);

            printf("%s / %s . %s -> %s\n", s_object_path, s_interface, s_key, s_parsed_string);
          }
        }
        u8_a_string_parsed = 1;
      }
      else if (i_dbus_type == DBUS_TYPE_OBJECT_PATH)
      {
        dbus_message_iter_get_basic(px_root_iterator,
                                    &s_parsed_object_path);
        //printf("Object_Path: >%s<\n", s_parsed_object_path);
      }
      else
      {
        //printf("Basic type %c\n", i_dbus_type);
      }
    }
    else
    {
      //printf("Unhandled type %c\n", i_dbus_type);
    }
  }
  while (dbus_message_iter_next(px_root_iterator));
  //printf("%c\n", dbus_message_iter_get_arg_type(px_root_iterator));
}


static void vReflectInterfaceStates(void)
{
  DBusMessage* px_dbus_msg_sent = NULL;
  DBusMessage* px_dbus_msg_reply = NULL;
  DBusError x_dbus_error;
  DBusMessageIter x_reply_iterator;
 
  dbus_error_init(&x_dbus_error);
  
  
  
  px_dbus_msg_sent =
    dbus_message_new_method_call("org.bluez",
                                 "/",
                                 "org.freedesktop.DBus.ObjectManager",
                                 "GetManagedObjects");

  if (px_dbus_msg_sent == NULL)
  {
    return;
  }

  px_dbus_msg_reply =
    dbus_connection_send_with_reply_and_block(gpx_dbus_system_conn,
                                              px_dbus_msg_sent,
                                              2000,
                                              &x_dbus_error);

  if (dbus_error_is_set(&x_dbus_error) || px_dbus_msg_reply == NULL)
  {
    if (px_dbus_msg_sent != NULL)
    {
      dbus_message_unref(px_dbus_msg_sent);
    }
    if (px_dbus_msg_reply != NULL)
    {
      dbus_message_unref(px_dbus_msg_reply);
    }
    return;
  }
  dbus_message_iter_init(px_dbus_msg_reply, &x_reply_iterator);

  //dbus_message_iter_get_arg_type(&x_reply_iterator);
  vExtractIteratorDataRec(&x_reply_iterator, 0, DBUS_TYPE_INVALID, "", "", "");
}


uint8_t u8LibRuuviTagInit(char* s_listen_on, char* s_listen_to)
{
  uint8_t u8_result = 0;
  
  vInitSemaphores();
  u8_result = u8InitSystemDbusConnection();
  ASSERT_RESULT(u8_result, LIBRUUVITAG_RES_OK);
  vCreateDbusMainLoopThread();
  vSubscribeBluezInterfaceMessages();
  // vReflectInterfaceStates() needs to be called before
  // setting main loop running because otherwise
  // there is a deadlock since both sender and the
  // main loop receiver both try the same lock.
  sem_wait(&gx_msg_processing_semaphore);
  vReflectInterfaceStates();
  sem_post(&gx_msg_processing_semaphore);
  vSetDbusMainLoopThreadRunning();
  
  return LIBRUUVITAG_RES_OK;
}


uint8_t u8LibRuuviTagDeinit(void)
{
  printf("Libruuvitag stopping\n");

  sem_wait(&gx_msg_processing_semaphore);
  pthread_cancel(gt_glib_main_loop_thread);
  pthread_join(gt_glib_main_loop_thread, NULL);
  dbus_connection_unref(gpx_dbus_system_conn);


  return 0;
}
