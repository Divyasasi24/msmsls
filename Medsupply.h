#ifndef MEDSUPPLY_H
#define MEDSUPPLY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ---------- Data storage files (binary format unchanged from array version) ---------- */
#define ITEMS_FILE    "inventory.dat"
#define REQUESTS_FILE "requests.dat"
#define HISTORY_FILE  "history.dat"
#define USERS_FILE    "users.dat"

/* ---------- User roles / login settings ---------- */
#define ROLE_ADMIN 1
#define ROLE_STAFF 2
#define MAX_LOGIN_ATTEMPTS 3
#define MIN_PASSWORD_LEN   6
#define DEFAULT_ADMIN_USER "admin"
#define DEFAULT_ADMIN_PASS "admin123"

/* ---------- Request status codes ---------- */
#define REQ_PENDING  0
#define REQ_APPROVED 1
#define REQ_REJECTED 2
#define REQ_ISSUED   3

/* ---------- Record structures ---------- */
typedef struct {
    int   id;
    char  name[50];
    char  category[30];
    int   quantity;
    float price;
    char  expiry[11];      /* YYYY-MM-DD */
    int   min_level;       /* reorder / low-stock threshold */
} Item;

typedef struct {
    int  req_id;
    int  item_id;
    char requester[40];
    int  qty;
    int  status;
    char date[11];
    char remarks[60];
    char created_by[24];   /* username that created the request */
} Request;

typedef struct {
    int  dist_id;
    int  req_id;
    int  item_id;
    char item_name[50];
    char recipient[40];
    int  qty;
    char date[11];
    char issued_by[24];    /* username that issued the supplies */
} Distribution;

typedef struct {
    char username[24];
    char salt[17];
    char hash[17];         /* salted, iterated hash - never the plain password */
    int  role;             /* ROLE_ADMIN or ROLE_STAFF */
} User;

/* ---------- Singly linked list (replaces the dynamic arrays) ----------
 * One generic list is used for all four record types.  Each node owns a heap
 * copy of its record; read it with  Item *it = node->data;  (void* converts
 * implicitly in C).  Always walk a list with:
 *       for (Node *n = list.head; n; n = n->next) ...
 */
typedef struct Node {
    void        *data;     /* heap copy of one Item / Request / Distribution / User */
    struct Node *next;
} Node;

typedef struct {
    Node *head;
    Node *tail;            /* kept so appending is O(1) */
    int   count;
} List;

extern User g_current_user;    /* the logged-in user (set by auth_login) */

/* ---------- linkedlist.c ---------- */
void list_init(List *l);                                   /* empty list */
int  list_append(List *l, const void *rec, size_t size);   /* copies rec; 1 = ok, 0 = out of memory */
void list_remove(List *l, Node *target);                   /* unlink + free one node */
void list_clear(List *l);                                  /* free every node, leave list empty */

/* ---------- utils.c ---------- */
void  read_line(const char *prompt, char *buf, int size);
int   read_int(const char *prompt, int min, int max);
float read_float(const char *prompt, float min, float max);
int   confirm(const char *prompt);
void  get_today(char *out);                 /* YYYY-MM-DD */
int   valid_date(const char *s);
int   days_until(const char *date);         /* days from today to date */
int   contains_ci(const char *text, const char *key);

/* ---------- filehandler.c (File Handling layer) ----------
 * Load functions return a List (empty when the file is missing/empty/corrupt).
 * The caller must list_clear() it.  Save functions write the whole list. */
List fh_load_items(void);
int  fh_save_items(const List *l);
List fh_load_requests(void);
int  fh_save_requests(const List *l);
List fh_load_history(void);
int  fh_append_history(const Distribution *rec);
List fh_load_users(void);
int  fh_save_users(const List *l);
void fh_init_files(void);

/* ---------- auth.c (login & user management) ---------- */
void        auth_init(void);                 /* creates default admin if no users exist */
int         auth_login(void);                /* 1 = logged in, 0 = too many failed attempts */
int         require_role(int role);          /* 1 = allowed, 0 = prints "Access denied" */
const char *role_name(int role);
void        read_password(const char *prompt, char *buf, int size);
void        change_own_password(void);
void        user_management_menu(void);

/* ---------- inventory.c ---------- */
void  print_item_header(void);
void  print_item(const Item *it);
Node *find_item(const List *l, int id);      /* node or NULL */
void  inventory_menu(int role);
void  add_item(void);
void  view_items(void);
void  search_item(void);
void  delete_item(void);

/* ---------- stock.c ---------- */
void stock_menu(void);
void update_stock(void);
void verify_stock(void);

/* ---------- requests.c ---------- */
void  requests_menu(int role);
void  create_request(void);
void  list_requests(int status_filter);
void  approve_request(void);
void  reject_request(void);
const char *status_name(int s);
Node *find_request(const List *l, int req_id);               /* node or NULL */
int   reserved_qty(const List *reqs, int item_id);

/* ---------- distribution.c ---------- */
void distribution_menu(int role);
void issue_supplies(void);
void view_history(void);

#endif