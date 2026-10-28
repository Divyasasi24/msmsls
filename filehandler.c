/* File Handling layer: all reads/writes of the Data Storage Files live here.
 * The binary file format is identical to the array version: a plain sequence of
 * fixed-size records. Only the in-memory structure changed (array -> linked list). */
#include "medsupply.h"

/* Generic loader: reads every complete record of `size` bytes into a new list.
 * Returns an empty list if the file is missing, too small, or unreadable. */
static List load_all(const char *file, size_t size)
{
    List l;
    FILE *fp = fopen(file, "rb");
    long bytes;
    int i, count;
    void *rec;

    list_init(&l);
    if (!fp) return l;
    fseek(fp, 0, SEEK_END);
    bytes = ftell(fp);
    rewind(fp);
    if (bytes < (long)size) { fclose(fp); return l; }

    count = (int)(bytes / (long)size);
    rec = malloc(size);
    if (!rec) { fclose(fp); return l; }

    for (i = 0; i < count; i++) {
        if (fread(rec, size, 1, fp) != 1 || !list_append(&l, rec, size)) {
            printf("Warning: could not fully read %s\n", file);
            list_clear(&l);
            break;
        }
    }
    free(rec);
    fclose(fp);
    return l;
}

/* Generic saver: overwrites the file with every node's record, head to tail. */
static int save_all(const char *file, const List *l, size_t size)
{
    FILE *fp = fopen(file, "wb");
    const Node *n;

    if (!fp) { printf("Error: cannot write to %s\n", file); return 0; }
    for (n = l->head; n; n = n->next) {
        if (fwrite(n->data, size, 1, fp) != 1) {
            printf("Error: write failed for %s\n", file);
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return 1;
}

List fh_load_items(void)                 { return load_all(ITEMS_FILE, sizeof(Item)); }
int  fh_save_items(const List *l)        { return save_all(ITEMS_FILE, l, sizeof(Item)); }

List fh_load_requests(void)              { return load_all(REQUESTS_FILE, sizeof(Request)); }
int  fh_save_requests(const List *l)     { return save_all(REQUESTS_FILE, l, sizeof(Request)); }

List fh_load_users(void)                 { return load_all(USERS_FILE, sizeof(User)); }
int  fh_save_users(const List *l)        { return save_all(USERS_FILE, l, sizeof(User)); }

List fh_load_history(void)               { return load_all(HISTORY_FILE, sizeof(Distribution)); }

int fh_append_history(const Distribution *rec)
{
    FILE *fp = fopen(HISTORY_FILE, "ab");
    if (!fp) { printf("Error: cannot open %s\n", HISTORY_FILE); return 0; }
    fwrite(rec, sizeof *rec, 1, fp);
    fclose(fp);
    return 1;
}

/* Create the data files if they do not exist yet */
void fh_init_files(void)
{
    const char *files[] = { ITEMS_FILE, REQUESTS_FILE, HISTORY_FILE, USERS_FILE };
    int i;
    for (i = 0; i < 4; i++) {
        FILE *fp = fopen(files[i], "ab");
        if (fp) fclose(fp);
        else printf("Warning: cannot create %s\n", files[i]);
    }
}