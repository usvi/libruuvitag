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


lrt_llist_node* pxLrtLlistFindNode(lrt_llist_head* px_list,
                                   int i_node_op,
                                   int (*iCompareNodes)(void*, void*),
                                   void* v_search_node_data)
{
  lrt_llist_node* px_node_iterator = NULL;
  lrt_llist_node* px_node_found = NULL;

  if (i_node_op == LRT_LLIST_FIND_EQUAL)
  {
    px_node_iterator = px_list->px_first_node;
    
    while (px_node_iterator != NULL)
    {
      if (iCompareNodes(px_node_iterator, v_search_node_data) == 0)
      {
        return px_node_iterator;
      }
      px_node_iterator = px_node_iterator->px_next_node;
    }

    return NULL;
  }
  else if (i_node_op == LRT_LLIST_FIND_LOWEST)
  {
    px_node_iterator = px_list->px_first_node;
    px_node_found = px_node_iterator;

    while (px_node_iterator != NULL)
    {
      if (iCompareNodes(px_node_iterator, px_node_found) < 0)
      {
        px_node_found = px_node_iterator;
      }
      px_node_iterator = px_node_iterator->px_next_node;
    }

    return px_node_found;
  }
  else if (i_node_op == LRT_LLIST_FIND_HIGHEST)
  {
    px_node_iterator = px_list->px_first_node;
    px_node_found = px_node_iterator;

    while (px_node_iterator != NULL)
    {
      if (iCompareNodes(px_node_iterator, px_node_found) > 0)
      {
        px_node_found = px_node_iterator;
      }
      px_node_iterator = px_node_iterator->px_next_node;
    }

    return px_node_found;
  }

  // Falltrough, should not really happen
  return NULL;
}


void vLrtLlistApplyFunc(lrt_llist_head* px_list,
                        int (*iApplyFunc)(void*))
{
  lrt_llist_node* px_node_iterator = NULL;

  px_node_iterator = px_list->px_first_node;
  
  while (px_node_iterator != NULL)
  {
    iApplyFunc(px_node_iterator);
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
