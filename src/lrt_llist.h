#ifndef _LRT_LLIST_H_
#define _LRT_LLIST_H_

/*
#define LRT_LLIST_UNKNOWN         (0)
#define LRT_LLIST_NO_OP           (1)
#define LRT_LLIST_ADDED           (2)
#define LRT_LLIST_MODIFIED        (3)
#define LRT_LLIST_REMOVED         (4)
*/

#define LRT_LLIST_FIND_EQUAL           (1)
#define LRT_LLIST_FIND_LOWEST          (2)
#define LRT_LLIST_FIND_HIGHEST         (3)


typedef struct lrt_llist_node lrt_llist_node;
typedef struct lrt_llist_head lrt_llist_head;

struct lrt_llist_node
{
  lrt_llist_node* px_prev_node;
  lrt_llist_node* px_next_node;
};

struct lrt_llist_head
{
  lrt_llist_node* px_first_node;
  lrt_llist_node* px_last_node;
};


lrt_llist_head* pxLrtLlistNew(void);

lrt_llist_node* pxLrtLlistFindNode(lrt_llist_head* px_list,
                                   int i_node_op,
                                   int (*iCompareNodes)(lrt_llist_node*, void*),
                                   void* v_search_node_data);

void vLrtLlistApplyFunc(lrt_llist_head* px_list,
                        int (*iApplyFunc)(lrt_llist_node*, void*),
                        void* pv_user_data);


void vLrtLlistAddNode(lrt_llist_head* px_list,
                      void* v_node_data);

void vLrtLlistFreeNode(lrt_llist_head* px_list,
                       void* v_node_data);

void vLrtLlistFreeAll(lrt_llist_head* px_list);


#endif // #ifndef	_LRT_LLIST_H_
