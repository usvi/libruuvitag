#include "lrt_llist.h"

#include "libruuvitag.h"
#include "lrt_linux_dbus_bluez.h"

#include <stdlib.h>


lrt_llist_head* pxLrtLlistNew(void)
{
  void* pv_malloc_test = NULL;
  lrt_llist_head* px_new_list = NULL;

  pv_malloc_test = malloc(sizeof(lrt_llist_head));

  if (pv_malloc_test == NULL)
  {
    return NULL;
  }
  px_new_list = (lrt_llist_head*)pv_malloc_test;
  px_new_list->px_first_node = NULL;
  px_new_list->px_last_node = NULL;

  return px_new_list;
}


lrt_llist_node* pxLrtLlistLowOrHighSearch(lrt_llist_head* px_list,
                                          uint8_t (*u8CompareNodes)(lrt_llist_node*, lrt_llist_node*))
{
  lrt_llist_node* px_node_iterator = NULL;
  lrt_llist_node* px_node_found = NULL;

  px_node_iterator = px_list->px_first_node;
  
  // Find first valid node as base
  while (px_node_iterator != NULL)
  {
    if (u8CompareNodes(px_node_iterator, px_node_iterator) == LRT_LLIST_COMPARE_EQUAL)
    {
      px_node_found = px_node_iterator;

      break;
    }
    px_node_iterator = px_node_iterator->px_next_node;
  }

  if (px_node_found == NULL)
  {
    return NULL;
  }
  // Actual search begin
  px_node_iterator = px_node_found->px_next_node;
  
  while (px_node_iterator != NULL)
  {
    if (u8CompareNodes(px_node_found, px_node_iterator) == LRT_LLIST_COMPARE_RIGHT_WINS)
    {
      px_node_found = px_node_iterator;
    }
    px_node_iterator = px_node_iterator->px_next_node;
  }
  
  return px_node_found;
}


lrt_llist_node* pxLrtLlistEqualParamSearch(lrt_llist_head* px_list,
                                           uint8_t (*u8CompareNodes)(lrt_llist_node*, void*),
                                           void* v_search_node_data)
{
  lrt_llist_node* px_node_iterator = NULL;

  px_node_iterator = px_list->px_first_node;
    
  while (px_node_iterator != NULL)
  {
    if (u8CompareNodes(px_node_iterator, v_search_node_data) == LRT_LLIST_COMPARE_EQUAL)
    {
      return px_node_iterator;
    }
    px_node_iterator = px_node_iterator->px_next_node;
  }

  // Falltrough
  return NULL;
}


void vLrtLlistApplyFunc(lrt_llist_head* px_list,
                        uint8_t (*u8ApplyFunc)(lrt_llist_node*, void*),
                        void* pv_user_data)
{
  lrt_llist_node* px_node_iterator = NULL;

  px_node_iterator = px_list->px_first_node;
  
  while (px_node_iterator != NULL)
  {
    u8ApplyFunc(px_node_iterator, pv_user_data);
    px_node_iterator = px_node_iterator->px_next_node;
  }
}


void vLrtLlistAddNode(lrt_llist_head* px_list,
                      void* v_node_data)
{
  lrt_llist_node* px_add_node = NULL;

  px_add_node = (lrt_llist_node*)v_node_data;

  // Could be empty list alltogether
  if (px_list->px_first_node == NULL)
  {
    px_list->px_first_node = px_add_node;
    px_list->px_last_node = px_add_node;
    px_add_node->px_prev_node = NULL;
    px_add_node->px_next_node = NULL;
  }
  else // Add as last
  {
    px_add_node->px_prev_node = px_list->px_last_node;
    px_add_node->px_next_node = NULL;
    px_list->px_last_node->px_next_node = px_add_node;
    px_list->px_last_node = px_add_node;
  }
}


void vLrtLlistFreeNode(lrt_llist_head* px_list,
                       void* v_node_data)
{
  lrt_llist_node* px_del_node = NULL;

  px_del_node = (lrt_llist_node*)v_node_data;

  if (px_del_node->px_prev_node != NULL)
  {
    px_del_node->px_prev_node->px_next_node = px_del_node->px_next_node;
  }
  if (px_del_node->px_next_node != NULL)
  {
    px_del_node->px_next_node->px_prev_node = px_del_node->px_prev_node;
  }
  if (px_list->px_first_node == px_del_node)
  {
    px_list->px_first_node = px_del_node->px_next_node;
  }
  if (px_list->px_last_node == px_del_node)
  {
    px_list->px_last_node = px_del_node->px_prev_node;
  }
  free(px_del_node);
}


void vLrtLlistFreeAll(lrt_llist_head* px_list)
{
  lrt_llist_node* px_node_iterator = NULL;
  lrt_llist_node* px_node_this = NULL;

  px_node_iterator = px_list->px_first_node;
  
  while (px_node_iterator != NULL)
  {
    px_node_this = px_node_iterator;
    px_node_iterator = px_node_iterator->px_next_node;
    free(px_node_this);
  }
  free(px_list);
}
