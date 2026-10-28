#include "medsupply.h"

void update_stock(void)
{
    if (!require_role(ROLE_ADMIN)) return;
    int id, mode, qty;
    List items = fh_load_items();
    Node *n;
    Item *it;

    if (items.count == 0) { printf("Inventory is empty. Add items first.\n"); list_clear(&items); return; }

    id = read_int("Enter item ID: ", 1, 100000000);
    n = find_item(&items, id);
    if (!n) { printf("Item not found.\n"); list_clear(&items); return; }
    it = n->data;

    print_item_header(); print_item(it);
    printf("\n1. Add stock (received)\n2. Remove stock (damaged/expired/adjust)\n3. Set exact quantity\n");
    mode = read_int("Choice: ", 1, 3);

    if (mode == 1) {
        qty = read_int("Quantity to add: ", 1, 10000000);
        if ((long)it->quantity + qty > 100000000L) { printf("Quantity too large.\n"); list_clear(&items); return; }
        it->quantity += qty;
    } else if (mode == 2) {
        qty = read_int("Quantity to remove: ", 1, 10000000);
        if (qty > it->quantity) { printf("Cannot remove more than current stock (%d).\n", it->quantity); list_clear(&items); return; }
        it->quantity -= qty;
    } else {
        it->quantity = read_int("New quantity: ", 0, 100000000);
    }

    if (fh_save_items(&items)) printf("Stock updated. New quantity of '%s': %d\n", it->name, it->quantity);
    list_clear(&items);
}

void verify_stock(void)
{
    if (!require_role(ROLE_ADMIN)) return;
    int low = 0, expired = 0, soon = 0, changed = 0;
    List items = fh_load_items();
    Node *n;

    if (items.count == 0) { printf("Inventory is empty.\n"); list_clear(&items); return; }

    printf("\n===== STOCK VERIFICATION =====\n");
    printf("%-5s %-24s %8s %-11s %s\n", "ID", "Name", "Qty", "Expiry", "Status");
    printf("--------------------------------------------------------------------\n");
    for (n = items.head; n; n = n->next) {
        const Item *it = n->data;
        int d = days_until(it->expiry);
        char status[80] = "";
        if (d < 0)               { strcat(status, "EXPIRED "); expired++; }
        else if (d <= 30)        { strcat(status, "EXPIRES-SOON "); soon++; }
        if (it->quantity == 0)   { strcat(status, "OUT-OF-STOCK "); low++; }
        else if (it->quantity <= it->min_level) { strcat(status, "LOW-STOCK "); low++; }
        if (status[0] == '\0') strcpy(status, "OK");
        printf("%-5d %-24.24s %8d %-11s %s\n", it->id, it->name, it->quantity, it->expiry, status);
    }
    printf("\nSummary: %d low/out of stock, %d expired, %d expiring within 30 days.\n", low, expired, soon);

    /* Physical count reconciliation */
    if (confirm("\nReconcile with a physical count? (y/n): ")) {
        for (;;) {
            int id = read_int("Item ID to reconcile (0 to finish): ", 0, 100000000);
            int counted;
            Item *it;
            if (id == 0) break;
            n = find_item(&items, id);
            if (!n) { printf("  Item not found.\n"); continue; }
            it = n->data;
            counted = read_int("  Physical count: ", 0, 100000000);
            if (counted == it->quantity) {
                printf("  Verified: record matches physical count (%d).\n", counted);
            } else {
                printf("  MISMATCH: record=%d, counted=%d (difference %+d)\n",
                       it->quantity, counted, counted - it->quantity);
                if (confirm("  Correct the record to the physical count? (y/n): ")) {
                    it->quantity = counted; changed = 1;
                    printf("  Record corrected.\n");
                }
            }
        }
        if (changed && fh_save_items(&items)) printf("Corrections saved.\n");
    }
    list_clear(&items);
}

void stock_menu(void)
{
    int c;
    do {
        printf("\n===== STOCK =====\n1. Update\n2. Verify\n0. Back\n");
        c = read_int("Choice: ", 0, 2);
        if (c == 1) update_stock();
        else if (c == 2) verify_stock();
    } while (c != 0);
}