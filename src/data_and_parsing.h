#ifndef _LIBRUUVITAG_DATA_AND_PARSING_H_
#define _LIBRUUVITAG_DATA_AND_PARSING_H_


#include <dbus/dbus.h>



typedef struct dbus_llist_node_type dbus_llist_node_type;
typedef struct dbus_llist_type dbus_llist_type;


struct dbus_llist_node_type
{
  int i_dbus_data_type;
  void* pv_data;
  dbus_llist_node_type* px_next;
};

struct dbus_llist_type
{
  dbus_llist_node_type* px_first;
  dbus_llist_node_type* px_last;
};


dbus_llist_type* pxNewDbusLinkedList(void);
void vPrintDbusLinkedList(dbus_llist_type* px_dbus_llist);
void vAddDbusLinkedList(dbus_llist_type* px_dbus_llist, DBusMessageIter* px_dbus_msg_iter);
void vDeleteDbusLinkedList(dbus_llist_type* px_dbus_llist);


void vExtractDbusMsgData(DBusMessage* px_dbus_msg,
                         char* s_search_object_path,
                         char* s_search_interface,
                         char* s_search_key,
                         dbus_llist_type* px_dbus_llist);


#endif // #ifndef _LIBRUUVITAG_DATA_AND_PARSING_H_
