#include "medsupply.h"

void issue_supplies(void)
{
    if (!require_role(ROLE_STAFF)) return;
    int id, maxid = 0, any = 0;
    List reqs = fh_load_requests();
    List items, hist;
    Node *n, *rn, *in;
    Request *r;
    Item *it;
    Distribution d;

    list_requests(REQ_APPROVED);
    for (n = reqs.head; n; n = n->next)           /* anything approved at all? */
        if (((Request *)n->data)->status == REQ_APPROVED) any = 1;
    if (!any) { list_clear(&reqs); return; }

    id = read_int("Request ID to issue (0 to cancel): ", 0, 100000000);
    if (id == 0) { list_clear(&reqs); return; }
    rn = find_request(&reqs, id);
    if (!rn || ((Request *)rn->data)->status != REQ_APPROVED) {
        printf("No approved request with that ID.\n"); list_clear(&reqs); return;
    }
    r = rn->data;

    items = fh_load_items();
    in = find_item(&items, r->item_id);
    if (!in) { printf("Item no longer exists.\n"); list_clear(&items); list_clear(&reqs); return; }
    it = in->data;
    if (days_until(it->expiry) < 0) {
        printf("Cannot issue: '%s' is expired (%s).\n", it->name, it->expiry);
        list_clear(&items); list_clear(&reqs); return;
    }
    if (it->quantity < r->qty) {
        printf("Insufficient stock: have %d, need %d. Update stock first.\n", it->quantity, r->qty);
        list_clear(&items); list_clear(&reqs); return;
    }

    /* next distribution id */
    hist = fh_load_history();
    for (n = hist.head; n; n = n->next)
        if (((Distribution *)n->data)->dist_id > maxid) maxid = ((Distribution *)n->data)->dist_id;
    list_clear(&hist);

    memset(&d, 0, sizeof d);
    d.dist_id = maxid + 1;
    d.req_id  = r->req_id;
    d.item_id = it->id;
    d.qty     = r->qty;
    snprintf(d.item_name, sizeof d.item_name, "%s", it->name);
    snprintf(d.recipient, sizeof d.recipient, "%s", r->requester);
    get_today(d.date);
    snprintf(d.issued_by, sizeof d.issued_by, "%s", g_current_user.username);

    it->quantity -= r->qty;
    r->status = REQ_ISSUED;
    strcpy(r->remarks, "Issued");

    if (fh_save_items(&items) && fh_save_requests(&reqs) && fh_append_history(&d))
        printf("Issued %d x %s to %s. Remaining stock: %d\n",
               d.qty, d.item_name, d.recipient, it->quantity);
    list_clear(&items); list_clear(&reqs);
}

void view_history(void)
{
    int total = 0;
    List hist = fh_load_history();
    Node *n;

    if (hist.count == 0) { printf("No distribution history yet.\n"); list_clear(&hist); return; }

    printf("\n%-6s %-6s %-24s %-20s %6s %-11s %s\n", "DistID", "ReqID", "Item", "Recipient", "Qty", "Date", "IssuedBy");
    printf("-----------------------------------------------------------------------------\n");
    for (n = hist.head; n; n = n->next) {
        const Distribution *h = n->data;
        printf("%-6d %-6d %-24.24s %-20.20s %6d %-11s %s\n",
               h->dist_id, h->req_id, h->item_name, h->recipient, h->qty, h->date, h->issued_by);
        total += h->qty;
    }
    printf("\n%d distributions, %d units issued in total.\n", hist.count, total);
    list_clear(&hist);
}

void distribution_menu(int role)
{
    int c;
    if (role == ROLE_ADMIN) {
        do {
            printf("\n===== DISTRIBUTION (REPORTS) =====\n1. History\n0. Back\n");
            c = read_int("Choice: ", 0, 1);
            if (c == 1) view_history();
        } while (c != 0);
    } else {
        do {
            printf("\n===== DISTRIBUTION =====\n1. Issue\n2. History\n0. Back\n");
            c = read_int("Choice: ", 0, 2);
            if (c == 1) issue_supplies();
            else if (c == 2) view_history();
        } while (c != 0);
    }
}