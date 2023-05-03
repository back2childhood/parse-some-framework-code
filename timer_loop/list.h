/* ************************************************************************
> File Name:     list.h
> Author:        Qian JiLi
> mail:          193937157@qq.com
> Created Time:  Wed May  3 22:48:03 2023
> Description:
 ************************************************************************/
#if !defined(_BLKID_LIST_H) && !defined(LIST_HEAD)
#define _BLKID_LIST_H
#ifdef __cplusplus
extern "C" {
#endif
struct list_head {
  struct list_head *next, *prev;
};
#define LIST_HEAD_INIT(name)                                                   \
  { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)
#define INIT_LIST_HEAD(ptr)                                                    \
  do {                                                                         \
    (ptr)->next = (ptr);                                                       \
    (ptr)->prev = (ptr);                                                       \
  } while (0)
static inline void __list_add(struct list_head *entry, struct list_head *prev,
                              struct list_head *next) {
  next->prev = entry;
  entry->next = next;
  entry->prev = prev;
  prev->next = entry;
}
static inline void list_add(struct list_head *entry, struct list_head *head) {
  __list_add(entry, head, head->next);
}
static inline void list_add_tail(struct list_head *entry,
                                 struct list_head *head) {
  __list_add(entry, head->prev, head);
}
static inline void __list_del(struct list_head *prev, struct list_head *next) {
  next->prev = prev;
  prev->next = next;
}
static inline void list_del(struct list_head *entry) {
  __list_del(entry->prev, entry->next);
}
static inline void list_del_init(struct list_head *entry) {
  __list_del(entry->prev, entry->next);
  INIT_LIST_HEAD(entry);
}
static inline void list_move_tail(struct list_head *list,
                                  struct list_head *head) {
  __list_del(list->prev, list->next);
  list_add_tail(list, head);
}
static inline int list_empty(struct list_head *head) {
  return head->next == head;
}
static inline void list_replace(struct list_head *old,
                                struct list_head *new_node) {
  new_node->next = old->next;
  new_node->next->prev = new_node;
  new_node->prev = old->prev;
  new_node->prev->next = new_node;
}
#define list_first_entry(ptr, type, member)                                    \
  list_entry((ptr)->next, type, member)
static inline void list_replace_init(struct list_head *old,
                                     struct list_head *new_node) {
  list_replace(old, new_node);
  INIT_LIST_HEAD(old);
}
#define list_entry(ptr, type, member)                                          \
  ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))
#define list_for_each(pos, head)                                               \
  for (pos = (head)->next; pos != (head); pos = pos->next)
#define list_for_each_safe(pos, pnext, head)                                   \
  for (pos = (head)->next, pnext = pos->next; pos != (head);                   \
       pos = pnext, pnext = pos->next)
#ifdef __cplusplus
}
#endif
#endif /* _BLKID_LIST_H */
