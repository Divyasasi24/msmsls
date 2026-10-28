#include "medsupply.h"

const char *status_name(int s)
{
    switch (s) {
        case REQ_PENDING:  return "PENDING";
        case REQ_APPROVED: return "APPROVED";
        case REQ_REJECTED: return "REJECTED";
        case REQ_ISSUED:   return "ISSUED";
    }
    return "UNKNOWN";
}

/* Returns the node of the request with this id, or NULL. */
Node *find_request(const List *l, int req_id)
{
    Node *n;
    for (n = l->head; n; n = n->next)
        if (((Request *)n->data)->req_id == req_id) return n;
    return NULL;
}

/* Quantity already promised to other approved (not yet issued) requests */
int reserved_qty(const List *reqs, int item_id)
{
    const Node *n;
    int total = 0;
    for (n = reqs->head; n; n = n->next) {
        const Request *r = n->data;
        if (r->item_id == item_id && r->status == REQ_APPROVED) total += r->qty;
    }
    return total;
}

void create_request(void)
{
    if (!require_role(ROLE_STAFF)) return;
    int id, qty, maxid = 0;
    List items = fh_load_items();
    List reqs;
    Node *n;
    Request r;

    if (items.count == 0) { printf("Inventory is empty.\n"); list_clear(&items); return; }

    id = read_int("Item ID requested: ", 1, 100000000);
    n = find_item(&items, id);
    if (!n) { printf("Item not found.\n"); list_clear(&items); return; }
    printf("Item: %s (in stock: %d)\n", ((Item *)n->data)->name, ((Item *)n->data)->quantity);
    list_clear(&items);

    memset(&r, 0, sizeof r);
    do { read_line("Requester / department: ", r.requester, sizeof r.requester); } while (r.requester[0] == '\0');
    qty = read_int("Quantity needed: ", 1, 10000000);

    reqs = fh_load_requests();
    for (n = reqs.head; n; n = n->next)
        if (((Request *)n->data)->req_id > maxid) maxid = ((Request *)n->data)->req_id;

    r.req_id  = maxid + 1;
    r.item_id = id;
    r.qty     = qty;
    r.status  = REQ_PENDING;
    get_today(r.date);
    snprintf(r.created_by, sizeof r.created_by, "%s", g_current_user.username);

    if (!list_append(&reqs, &r, sizeof r)) { printf("Out of memory.\n"); list_clear(&reqs); return; }
    if (fh_save_requests(&reqs)) printf("Request #%d created (PENDING).\n", r.req_id);
    list_clear(&reqs);
}

/* status_filter: -1 = all */
void list_requests(int status_filter)
{
    int shown = 0;
    List reqs = fh_load_requests();
    List items = fh_load_items();
    Node *n;

    for (n = reqs.head; n; n = n->next) {
        const Request *r = n->data;
        Node *in;
        if (status_filter >= 0 && r->status != status_filter) continue;
        if (!shown)
            printf("\n%-5s %-22s %-20s %6s %-10s %-11s %-12s %s\n",
                   "ReqID", "Item", "Requester", "Qty", "Status", "Date", "By", "Remarks");
        in = find_item(&items, r->item_id);
        printf("%-5d %-22.22s %-20.20s %6d %-10s %-11s %-12.12s %s\n",
               r->req_id,
               in ? ((Item *)in->data)->name : "(deleted item)",
               r->requester, r->qty,
               status_name(r->status), r->date, r->created_by, r->remarks);
        shown++;
    }
    if (!shown) printf("No requests to display.\n");
    list_clear(&reqs); list_clear(&items);
}

void approve_request(void)
{
    if (!require_role(ROLE_ADMIN)) return;
    int id, available;
    List reqs = fh_load_requests();
    List items;
    Node *rn, *in;
    Request *r;
    Item *it;

    list_requests(REQ_PENDING);
    if (reqs.count == 0) { list_clear(&reqs); return; }

    id = read_int("Request ID to approve (0 to cancel): ", 0, 100000000);
    if (id == 0) { list_clear(&reqs); return; }
    rn = find_request(&reqs, id);
    if (!rn || ((Request *)rn->data)->status != REQ_PENDING) {
        printf("No pending request with that ID.\n"); list_clear(&reqs); return;
    }
    r = rn->data;

    items = fh_load_items();
    in = find_item(&items, r->item_id);
    if (!in) { printf("Item no longer exists. Reject this request instead.\n"); list_clear(&items); list_clear(&reqs); return; }
    it = in->data;

    available = it->quantity - reserved_qty(&reqs, it->id);
    if (days_until(it->expiry) < 0) {
        printf("Cannot approve: '%s' is expired (%s).\n", it->name, it->expiry);
    } else if (r->qty > available) {
        printf("Cannot approve: requested %d but only %d available (stock %d minus reserved).\n",
               r->qty, available, it->quantity);
    } else {
        r->status = REQ_APPROVED;
        strcpy(r->remarks, "Approved");
        if (fh_save_requests(&reqs)) printf("Request #%d approved.\n", id);
    }
    list_clear(&items); list_clear(&reqs);
}

void reject_request(void)
{
    if (!require_role(ROLE_ADMIN)) return;
    int id;
    List reqs = fh_load_requests();
    Node *rn;
    Request *r;

    list_requests(REQ_PENDING);
    if (reqs.count == 0) { list_clear(&reqs); return; }

    id = read_int("Request ID to reject (0 to cancel): ", 0, 100000000);
    if (id == 0) { list_clear(&reqs); return; }
    rn = find_request(&reqs, id);
    if (!rn || ((Request *)rn->data)->status != REQ_PENDING) {
        printf("No pending request with that ID.\n"); list_clear(&reqs); return;
    }
    r = rn->data;
    read_line("Reason for rejection: ", r->remarks, sizeof r->remarks);
    if (r->remarks[0] == '\0') strcpy(r->remarks, "Rejected");
    r->status = REQ_REJECTED;
    if (fh_save_requests(&reqs)) printf("Request #%d rejected.\n", id);
    list_clear(&reqs);
}

void requests_menu(int role)
{
    int c;
    if (role == ROLE_ADMIN) {
        do {
            printf("\n===== REQUESTS (REVIEW) =====\n"
                   "1. View All Requests\n2. Approve\n3. Reject\n0. Back\n");
            c = read_int("Choice: ", 0, 3);
            switch (c) {
                case 1: list_requests(-1);  break;
                case 2: approve_request();  break;
                case 3: reject_request();   break;
            }
        } while (c != 0);
    } else {
        do {
            printf("\n===== REQUESTS =====\n"
                   "1. New Request\n2. View Request Status\n0. Back\n");
            c = read_int("Choice: ", 0, 2);
            switch (c) {
                case 1: create_request();   break;
                case 2: list_requests(-1);  break;
            }
        } while (c != 0);
    }
}