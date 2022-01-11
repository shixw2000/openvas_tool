#ifndef __LLIST_H__
#define __LLIST_H__


#ifdef __cplusplus
extern "C" {
#endif

struct LList {
    struct LList* prev;
    struct LList* next;
};

typedef struct LList* LList_t;

typedef int (*PComp)(const LList_t o1, const LList_t o2);
typedef void (*PFree)(LList_t); 
typedef int (*PWork)(void*, LList_t);


struct ListQueue {
    int m_cnt;
    PComp m_cmp;
    struct LList m_head;
};

typedef struct ListQueue* ListQueue_t; 

struct ListSet {
    int m_capacity;
    int m_size;
    PComp m_cmp;
    LList_t* m_data;
};

typedef struct ListSet* ListSet_t; 

extern int reset(LList_t item); 
extern int isEmpty(LList_t item); 

/* without pthread lock */
extern int initListQue(ListQueue_t root, PComp cmp);
extern int resetListQue(ListQueue_t root);
extern int freeListQue(ListQueue_t root, PFree pf);

extern int initListSet(ListSet_t arr, PComp cmp);
extern int freeListSet(ListSet_t arr, PFree pf);

/* push to tail, then order ascended by cmp */ 
extern int push_back(ListQueue_t root, LList_t item);

extern LList_t pop_head(ListQueue_t root);
extern LList_t get_head(ListQueue_t root);
extern int del_item(ListQueue_t root, LList_t item);

/* requeue the item */ 
extern int reque_list(ListQueue_t root, LList_t item);

extern int for_each(ListQueue_t root, PWork work, void*);


/* binary search in range [low, high) */
extern LList_t searchListSet(ListSet_t listset, const LList_t key);

extern int addToSet(ListSet_t listset, LList_t item);
extern LList_t delFromSet(ListSet_t listset, const LList_t key);

#ifdef __cplusplus
}
#endif 

#endif

