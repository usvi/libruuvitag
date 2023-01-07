
#include "data_and_parsing.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>

#include <dbus/dbus.h>


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


void vExtractDbusMsgData(DBusMessage* px_dbus_msg)
{
  DBusMessageIter x_reply_iterator;

  dbus_message_iter_init(px_dbus_msg, &x_reply_iterator);
  vExtractIteratorDataRec(&x_reply_iterator, 0, DBUS_TYPE_INVALID, "", "", "");
}
