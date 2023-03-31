
#include "lrt_data_and_parsing.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <dbus/dbus.h>


static uint8_t u8MatchIteratorData(char* s_iter_object_path,
                                   char* s_iter_interface,
                                   char* s_iter_key,
                                   char* s_search_object_path,
                                   char* s_search_interface,
                                   char* s_search_key)
{


  // If search is null, it is ok

  if ((s_search_object_path != NULL) &&
      ((s_iter_object_path == NULL) ||
       (strcmp(s_search_object_path, s_iter_object_path) != 0)))
  {
    return 0;
  }
        
  if ((s_search_interface != NULL) &&
      ((s_iter_interface == NULL) ||
       (strcmp(s_search_interface, s_iter_interface) != 0)))
  {
    return 0;
  }
        
  if ((s_search_key != NULL) &&
      ((s_iter_key == NULL) ||
       (strcmp(s_search_key, s_iter_key) != 0)))
  {
    return 0;
  }
  
  return 1;
}

                                   
dbus_llist_type* pxNewDbusLinkedList(void)
{
  dbus_llist_type* px_new_list;

  px_new_list = malloc(sizeof(*px_new_list));
  px_new_list->px_first = NULL;
  px_new_list->px_last = NULL;

  return px_new_list;
}


void vPrintDbusLinkedList(dbus_llist_type* px_dbus_llist)
{
  dbus_llist_node_type* px_print_node;

  if (px_dbus_llist == NULL)
  {
    return;
  }
  for (px_print_node = px_dbus_llist->px_first;
       px_print_node != NULL;
       px_print_node = px_print_node->px_next)
  {
    //printf("Linkedlist had type %c\n", px_print_node->i_dbus_data_type);
    if (px_print_node->i_dbus_data_type == DBUS_TYPE_STRING)
    {
      printf("%s\n", (char*)(px_print_node->pv_data));
    }
  }
}


void vAddDbusLinkedList(dbus_llist_type* px_dbus_llist, DBusMessageIter* px_dbus_msg_iter)
{
  int i_dbus_data_type;
  char* s_parsed_string = NULL;
  dbus_llist_node_type* px_new_node;
  
  if (px_dbus_llist == NULL)
  {
    return;
  }
  i_dbus_data_type = dbus_message_iter_get_arg_type(px_dbus_msg_iter);

  if (i_dbus_data_type == DBUS_TYPE_STRING)
  {
    dbus_message_iter_get_basic(px_dbus_msg_iter,
                                &s_parsed_string);

    px_new_node = malloc(sizeof(*px_new_node));
    px_new_node->i_dbus_data_type = i_dbus_data_type;
    px_new_node->pv_data = strdup(s_parsed_string);
    px_new_node->px_next = NULL;

    if ((px_dbus_llist->px_first == NULL) ||
        (px_dbus_llist->px_last == NULL))
    {
      px_dbus_llist->px_first = px_new_node;
      px_dbus_llist->px_last = px_new_node;
    }
    else
    {
      px_dbus_llist->px_last->px_next = px_new_node;
      px_dbus_llist->px_last = px_new_node;
    }
  }
}


void vDeleteDbusLinkedList(dbus_llist_type* px_dbus_llist)
{
  dbus_llist_node_type* px_delete_node;
  dbus_llist_node_type* px_next_node;

  if (px_dbus_llist == NULL)
  {
    return;
  }
  px_delete_node = px_dbus_llist->px_first;

  while (px_delete_node != NULL)
  {
    px_next_node = px_delete_node->px_next;

    if (px_delete_node->pv_data != NULL)
    {
      free(px_delete_node->pv_data);
    }
    free(px_delete_node);
    px_delete_node = px_next_node;
  }
  free(px_dbus_llist);
}


static void vExtractIteratorDataRec(DBusMessageIter* px_dbus_msg_iter,
                                    uint8_t u8_depth,
                                    int i_this_container_type,
                                    char* s_object_path,
                                    char* s_interface,
                                    char* s_key,
                                    char* s_search_object_path,
                                    char* s_search_interface,
                                    char* s_search_key,
                                    dbus_llist_type* px_dbus_llist)
{
  DBusMessageIter x_sub_iterator;
  int i_dbus_data_type = DBUS_TYPE_INVALID;
  char* s_parsed_object_path = NULL;
  char* s_parsed_interface = NULL;
  char* s_parsed_key = NULL;
  uint8_t u8_first_parsed = 0;
  
  do
  {
    i_dbus_data_type = dbus_message_iter_get_arg_type(px_dbus_msg_iter);

    if (dbus_type_is_container(i_dbus_data_type))
    {
      dbus_message_iter_recurse(px_dbus_msg_iter, &x_sub_iterator);
      vExtractIteratorDataRec(&x_sub_iterator,
                              u8_depth + 1,
                              i_dbus_data_type,
                              
                              ((s_parsed_object_path != NULL) ?
                               s_parsed_object_path :
                               s_object_path),

                              ((s_parsed_interface != NULL) ?
                               s_parsed_interface :
                               s_interface),

                              ((s_parsed_key != NULL) ?
                               s_parsed_key :
                               s_key),
                              
                              s_search_object_path,
                              s_search_interface,
                              s_search_key,
                              px_dbus_llist
                              );

    }
    else if (dbus_type_is_basic(i_dbus_data_type))
    {
      if ((u8_depth == 4) &&
          (u8_first_parsed == 0) &&
          (i_dbus_data_type == DBUS_TYPE_STRING))
      {
        dbus_message_iter_get_basic(px_dbus_msg_iter,
                                    &s_parsed_interface);
        u8_first_parsed = 1;
      }
      else if ((u8_first_parsed == 0) &&
               (i_dbus_data_type == DBUS_TYPE_STRING) &&
               (i_this_container_type == DBUS_TYPE_DICT_ENTRY))
      {
        dbus_message_iter_get_basic(px_dbus_msg_iter,
                                    &s_parsed_key);
        u8_first_parsed = 1;
        
      }
      else if (i_dbus_data_type == DBUS_TYPE_OBJECT_PATH)
      {
        dbus_message_iter_get_basic(px_dbus_msg_iter,
                                    &s_parsed_object_path);
        u8_first_parsed = 1;
      }
      else
      {
        // Actual store (if match)
        if (u8MatchIteratorData(s_object_path, s_interface, s_key,
                                s_search_object_path, s_search_interface, s_search_key))
        {
          //printf("%s / %s . %s -> %s\n", s_object_path, s_interface, s_key, s_parsed_string);
          vAddDbusLinkedList(px_dbus_llist, px_dbus_msg_iter);
        }
        u8_first_parsed = 1;
      }
    }
    else
    {
      // Unhandled type
    }
  }
  while (dbus_message_iter_next(px_dbus_msg_iter));
}


void vExtractDbusMsgData(DBusMessage* px_dbus_msg,
                         char* s_search_object_path,
                         char* s_search_interface,
                         char* s_search_key,
                         dbus_llist_type* px_dbus_llist)
{
  DBusMessageIter x_reply_iterator;

  dbus_message_iter_init(px_dbus_msg, &x_reply_iterator);
  vExtractIteratorDataRec(&x_reply_iterator, 0, DBUS_TYPE_INVALID, "", "", "",
                          s_search_object_path, s_search_interface, s_search_key,
                          px_dbus_llist);
}
