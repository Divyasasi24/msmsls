#include "medsupply.h"

void print_item_header(void)
{
    printf("\n%-5s %-24s %-14s %8s %10s %-11s %6s\n",
           "ID", "Name", "Category", "Qty", "Price", "Expiry", "MinLvl");
    printf("---------------------------------------------------------------------------------\n");
}

void print_item(const Item *it)
{
    printf("%-5d %-24.24s %-14.14s %8d %10.2f %-11s %6d\n",
           it->id, it->name, it->category, it->quantity, it->price, it->expiry, it->min_level);
}

/* Linear walk of the list; returns the node with this item id, or NULL. */
Node *find_item(const List *l, int id)
{
    Node *n;
    for (n = l->head; n; n = n->next)
        if (((Item *)n->data)->id == id) return n;
    return NULL;
}

void add_item(void)
{
    if (!require_role(ROLE_ADMIN)) return;
    int maxid = 0;
    List items = fh_load_items();
    Node *n;
    Item it;

    for (n = items.head; n; n = n->next)
        if (((Item *)n->data)->id > maxid) maxid = ((Item *)n->data)->id;

    memset(&it, 0, sizeof it);
    it.id = maxid + 1;
    do { read_line("Item name       : ", it.name, sizeof it.name); } while (it.name[0] == '\0');
    read_line("Category        : ", it.category, sizeof it.category);
    it.quantity  = read_int("Quantity        : ", 0, 10000000);
    it.price     = read_float("Unit price      : ", 0.0f, 10000000.0f);
    for (;;) {
        read_line("Expiry (YYYY-MM-DD): ", it.expiry, sizeof it.expiry);
        if (valid_date(it.expiry)) break;
        printf("  Invalid date format.\n");
    }
    it.min_level = read_int("Low-stock level : ", 0, 10000000);

    if (!list_append(&items, &it, sizeof it)) { printf("Out of memory.\n"); list_clear(&items); return; }
    if (fh_save_items(&items)) printf("Item added with ID %d.\n", it.id);
    list_clear(&items);
}

void view_items(void)
{
    List items = fh_load_items();
    Node *n;

    if (items.count == 0) { printf("Inventory is empty.\n"); list_clear(&items); return; }
    print_item_header();
    for (n = items.head; n; n = n->next) print_item(n->data);
    printf("\nTotal items: %d\n", items.count);
    list_clear(&items);
}

void search_item(void)
{
    int found = 0, choice;
    List items = fh_load_items();
    Node *n;

    if (items.count == 0) { printf("Inventory is empty.\n"); list_clear(&items); return; }

    printf("\n1. Search by ID\n2. Search by name / category\n");
    choice = read_int("Choice: ", 1, 2);

    if (choice == 1) {
        int id = read_int("Enter item ID: ", 1, 100000000);
        n = find_item(&items, id);
        if (n) { print_item_header(); print_item(n->data); found = 1; }
    } else {
        char key[50];
        read_line("Enter keyword: ", key, sizeof key);
        for (n = items.head; n; n = n->next) {
            const Item *it = n->data;
            if (contains_ci(it->name, key) || contains_ci(it->category, key)) {
                if (!found) print_item_header();
                print_item(it);
                found++;
            }
        }
    }
    if (!found) printf("No matching item found.\n");
    list_clear(&items);
}

void delete_item(void)
{
    if (!require_role(ROLE_ADMIN)) return;
    int id, pending = 0;
    List items = fh_load_items();
    List reqs;
    Node *n;

    if (items.count == 0) { printf("Inventory is empty.\n"); list_clear(&items); return; }

    id = read_int("Enter ID of item to delete: ", 1, 100000000);
    n = find_item(&items, id);
    if (!n) { printf("Item not found.\n"); list_clear(&items); return; }

    print_item_header(); print_item(n->data);

    /* Do not orphan open requests */
    reqs = fh_load_requests();
    for (Node *r = reqs.head; r; r = r->next) {
        const Request *rq = r->data;
        if (rq->item_id == id && (rq->status == REQ_PENDING || rq->status == REQ_APPROVED))
            pending++;
    }
    list_clear(&reqs);
    if (pending) {
        printf("Cannot delete: %d open request(s) (pending/approved) reference this item.\n", pending);
        list_clear(&items);
        return;
    }

    if (confirm("Delete this item permanently? (y/n): ")) {
        list_remove(&items, n);
        if (fh_save_items(&items)) printf("Item deleted.\n");
    } else printf("Cancelled.\n");
    list_clear(&items);
}

void inventory_menu(int role)
{
    int c;
    if (role == ROLE_ADMIN) {
        do {
            printf("\n===== INVENTORY =====\n"
                   "1. Add Item\n2. View All Items\n3. Search\n4. Delete\n0. Back\n");
            c = read_int("Choice: ", 0, 4);
            switch (c) {
                case 1: add_item();    break;
                case 2: view_items();  break;
                case 3: search_item(); break;
                case 4: delete_item(); break;
            }
        } while (c != 0);
    } else {
        do {
            printf("\n===== ITEM LOOKUP =====\n"
                   "1. View All Items\n2. Search\n0. Back\n");
            c = read_int("Choice: ", 0, 2);
            switch (c) {
                case 1: view_items();  break;
                case 2: search_item(); break;
            }
        } while (c != 0);
    }
}