/* Generic singly linked list used for items, requests, history and users. */
#include "medsupply.h"

void list_init(List *l)
{
    l->head = NULL;
    l->tail = NULL;
    l->count = 0;
}

/* Appends a copy of rec (size bytes) at the tail. Returns 1 on success. */
int list_append(List *l, const void *rec, size_t size)
{
    Node *n = malloc(sizeof *n);
    if (!n) return 0;
    n->data = malloc(size);
    if (!n->data) { free(n); return 0; }
    memcpy(n->data, rec, size);
    n->next = NULL;

    if (l->tail) l->tail->next = n;
    else         l->head = n;
    l->tail = n;
    l->count++;
    return 1;
}

/* Unlinks target from the list and frees it. Does nothing if not in the list. */
void list_remove(List *l, Node *target)
{
    Node *prev = NULL, *cur = l->head;

    while (cur && cur != target) { prev = cur; cur = cur->next; }
    if (!cur) return;

    if (prev) prev->next = cur->next;
    else      l->head = cur->next;
    if (l->tail == cur) l->tail = prev;
    l->count--;
    free(cur->data);
    free(cur);
}

void list_clear(List *l)
{
    Node *cur = l->head, *next;
    while (cur) {
        next = cur->next;
        free(cur->data);
        free(cur);
        cur = next;
    }
    list_init(l);
}