#include<string.h>
#include<stdlib.h>
#include"llist.h" 


#define DEF_LIST_ARRAY_SIZE  100


int reset(LList_t item) {
    item->prev = item;
    item->next = item;

    return 0;
}

int isEmpty(LList_t item) {
    return item->next == item ? 1 : 0;
}

static void delListItem(LList_t item) {
    item->prev->next = item->next;
    item->next->prev = item->prev;

    reset(item);
}

/* add the new item before the current one*/
static void addPrev(LList_t curr, LList_t newItr) {
    newItr->next = curr;
    newItr->prev = curr->prev;

    curr->prev->next = newItr;
    curr->prev = newItr;
}

/* add the new item after the current one*/
static void addNext(LList_t curr, LList_t newItr) {
    newItr->next = curr->next;
    newItr->prev = curr;

    curr->next->prev = newItr;
    curr->next = newItr;
}

int initListQue(ListQueue_t root, PComp cmp) {
    memset(root, 0, sizeof(struct ListQueue));
    
    reset(&root->m_head);
    root->m_cmp = cmp;
    root->m_cnt = 0;

    if (NULL != root->m_cmp) {
        return 0;
    } else {
        return -1;
    }
}

int resetListQue(ListQueue_t root) {
    reset(&root->m_head);
    root->m_cnt = 0;

    return 0;
}

int freeListQue(ListQueue_t root, PFree pf) {
    LList_t curr = NULL;

    while (root->m_head.next != &root->m_head) {
        curr = root->m_head.next;

        delListItem(curr);
        if (NULL != pf) {
            pf(curr);
        } else {
            free(curr);
        }
    }

    root->m_cnt = 0;
    return 0;
} 

int initListSet(ListSet_t arr, PComp cmp) { 
    memset(arr, 0, sizeof(struct ListSet));
    
    arr->m_data = calloc(DEF_LIST_ARRAY_SIZE, sizeof(LList_t));
    if (NULL != arr->m_data) {
        arr->m_cmp = cmp;
        arr->m_capacity = DEF_LIST_ARRAY_SIZE;
        
        return 0;
    } else {
        return -1;
    }
}

int freeListSet(ListSet_t arr, PFree pf) {
    int n = 0;
    
    if (NULL != arr->m_data) {
        for (n=0; n<arr->m_size; ++n) {
            if (NULL != arr->m_data[n]) {
                if (NULL != pf) {
                    pf(arr->m_data[n]);
                } else {
                    free(arr->m_data[n]);
                }

                arr->m_data[n] = NULL;
            }
        }
        
        free(arr->m_data);

        arr->m_data = NULL;
        arr->m_capacity = arr->m_size = 0;
    }

    return 0;
} 

/* push to tail, then order ascended by cmp */
int push_back(ListQueue_t root, LList_t item) {
    int ret = 0;
    LList_t curr = NULL;

    curr = &root->m_head;
    while (curr->prev != &root->m_head) {
        ret = root->m_cmp(curr->prev, item);
        if (0 >= ret) {
            /* the prev is less than or equal item */
            break;
        } else {
            curr = curr->prev;
        }
    }
    
    addPrev(curr, item); 
    ++root->m_cnt;

    return 0;
} 

/* pop head */
LList_t pop_head(ListQueue_t root) {
    LList_t curr = NULL;
    
    if (root->m_head.next != &root->m_head) {
        curr = root->m_head.next;

        delListItem(curr);
        --root->m_cnt;
    }

    return curr;
} 

LList_t get_head(ListQueue_t root) {
    LList_t curr = NULL;

    if (root->m_head.next != &root->m_head) {
        curr = root->m_head.next;
    }

    return curr;
}

int del_item(ListQueue_t root, LList_t item) {
    int ret = 0;

    if (item->next != item) {
        delListItem(item);
        --root->m_cnt;
    } else {
        ret = -1;
    }

    return ret;
}

/* requeue the item */ 
int reque_list(ListQueue_t root, LList_t item) {
    if (item->next != item) {
        del_item(root, item);
        push_back(root, item);
    }

    return 0;
} 

int for_each(ListQueue_t root, PWork work, void* ctx) {
    int ret = 0;
    LList_t next = NULL;
    LList_t curr = NULL;

    next = root->m_head.next;
    while (next != &root->m_head) {
        curr = next;
        next = next->next;
        
        ret = work(ctx, curr);
        if (0 != ret) {
            break;
        } 
    }

    return ret;
}

/* binary search in range [low, high) */
static LList_t binarySearch(LList_t arr[], int low, int high,
    const LList_t key, PComp cmp) {
    int mid = 0;
    int ret = 0;

    if (low < high) {
        mid = (low + high) / 2;
        
        ret = cmp(arr[mid], key);
        if (0 > ret) {
            return binarySearch(arr, mid+1, high, key, cmp);
        } else if (0 < ret) {
            return binarySearch(arr, low, mid, key, cmp);
        } else {
            return arr[mid];
        } 
    } else {
        return NULL;
    }
}

/* head.next is the least one */
LList_t searchListSet(ListSet_t listset, const LList_t key) {
    LList_t val = NULL;

    if (0 < listset->m_size) {
        val = binarySearch(listset->m_data, 0, listset->m_size, key, listset->m_cmp);
    }

    return val;
}

static int growArray(ListSet_t listset) {
    int ret = 0;
    int cnt = 0;
    void* p = NULL;

    if (listset->m_size == listset->m_capacity) {
        cnt = listset->m_capacity + DEF_LIST_ARRAY_SIZE;
        
        p = realloc(listset->m_data, sizeof(LList_t) * cnt);
        if (NULL != p) {
            listset->m_data = (LList_t*)p;
            listset->m_capacity = cnt;
        } else {
            /* if no memory, then unchaned */
            ret = -1;
        }
    }

    return ret; 
}

/* add one item to the set, ordered by cmp, and allow mutiple vals */
int addToSet(ListSet_t listset, LList_t item) {
    int ret = 0;
    int n = 0;
    
    ret = growArray(listset);
    if (0 != ret) {
        /* no memory */
        return ret;
    }

    for (n=listset->m_size-1; n>=0; --n) {
        ret = listset->m_cmp(listset->m_data[n], item);
        if (0 < ret) {
            /* curr is greater than item */
            listset->m_data[n+1] = listset->m_data[n];
        } else {
            /* curr is less than or equal to item, so insert after here */
            break;
        }
    }

    /* if n == -1, then item is the least one */
    listset->m_data[n+1] = item;
    ++listset->m_size;
    return 0;
}

LList_t delFromSet(ListSet_t listset, const LList_t key) {
    int ret = 0;
    int n = 0;
    LList_t curr = NULL;

    for (n=0; n<listset->m_size; ++n) {
        curr = listset->m_data[n];
        ret = listset->m_cmp(curr, key);
        if (0 > ret) {
            /* curr is less than key , go ahead */
            continue;
        } else if (0 == ret) {
            /* delete the n-th element */
            for (; n+1 < listset->m_size; ++n) { 
                listset->m_data[n] = listset->m_data[n+1]; 
            }

            listset->m_data[n] = NULL;
            --listset->m_size;
            return curr;
        } else {
            /* curr is greater than key, not found */
            break;
        }
    }

    /* not found */
    return NULL;
}

