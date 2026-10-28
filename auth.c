/*
 * Authentication and user management.
 *   - Two roles: ROLE_ADMIN and ROLE_STAFF
 *   - Passwords are never stored in plain text: each user has a random salt and
 *     an iterated 64-bit FNV-1a hash. (Good for a coursework/demo project; a
 *     production system should use bcrypt / scrypt / Argon2.)
 *   - Users are held in a singly linked list while the program works with them.
 *   - A default admin account (admin / admin123) is created on first run.
 */
#define _POSIX_C_SOURCE 200112L
#include "medsupply.h"
#include <stdint.h>

#ifdef _WIN32
  #include <conio.h>
  #include <io.h>
  #define STDIN_IS_TTY() _isatty(_fileno(stdin))
#else
  #include <termios.h>
  #include <unistd.h>
  #define STDIN_IS_TTY() isatty(STDIN_FILENO)
#endif

User g_current_user;

/* ------------------------------------------------------------------ helpers */
const char *role_name(int role)
{
    return role == ROLE_ADMIN ? "ADMIN" : role == ROLE_STAFF ? "STAFF" : "UNKNOWN";
}

int require_role(int role)
{
    if (g_current_user.role != role) {
        printf("Access denied: %s privileges required.\n", role_name(role));
        return 0;
    }
    return 1;
}

/* Reads a password; hides the typing when stdin is a real terminal. */
void read_password(const char *prompt, char *buf, int size)
{
    if (!STDIN_IS_TTY()) { read_line(prompt, buf, size); return; }

    printf("%s", prompt); fflush(stdout);
#ifdef _WIN32
    {
        int i = 0, ch;
        while ((ch = _getch()) != '\r' && ch != '\n') {
            if (ch == '\b') { if (i > 0) i--; }
            else if (i < size - 1) buf[i++] = (char)ch;
        }
        buf[i] = '\0';
        putchar('\n');
    }
#else
    {
        struct termios oldt, newt;
        int ok;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        ok = (fgets(buf, size, stdin) != NULL);
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        putchar('\n');
        if (!ok) exit(0);
        buf[strcspn(buf, "\n")] = '\0';
    }
#endif
}

static void hash_password(const char *salt, const char *pw, char out[17])
{
    uint64_t h = 1469598103934665603ULL;
    char data[128];
    int round;
    size_t i, n;

    snprintf(data, sizeof data, "%s:%s", salt, pw);
    n = strlen(data);
    for (round = 0; round < 5000; round++) {
        for (i = 0; i < n; i++) {
            h ^= (unsigned char)data[i];
            h *= 1099511628211ULL;
        }
        h ^= (uint64_t)round;
        h *= 1099511628211ULL;
    }
    snprintf(out, 17, "%016llx", (unsigned long long)h);
}

static void make_salt(char out[17])
{
    static const char hex[] = "0123456789abcdef";
    int i;
    for (i = 0; i < 16; i++) out[i] = hex[rand() % 16];
    out[16] = '\0';
}

static void set_password(User *u, const char *pw)
{
    make_salt(u->salt);
    hash_password(u->salt, pw, u->hash);
}

static int check_password(const User *u, const char *pw)
{
    char h[17];
    hash_password(u->salt, pw, h);
    return strcmp(h, u->hash) == 0;
}

static int eq_ci(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* Returns the node holding the user, or NULL if there is none. */
static Node *find_user(const List *l, const char *name)
{
    Node *n;
    for (n = l->head; n; n = n->next)
        if (eq_ci(((User *)n->data)->username, name)) return n;
    return NULL;
}

static int valid_username(const char *s)
{
    size_t i, n = strlen(s);
    if (n < 3 || n > 20) return 0;
    for (i = 0; i < n; i++)
        if (!isalnum((unsigned char)s[i]) && s[i] != '_') return 0;
    return 1;
}

/* Asks for a new password twice. Returns 1 and fills pw on success. */
static int read_new_password(char *pw, int size)
{
    char again[64];
    read_password("New password (min 6 chars): ", pw, size);
    if (strlen(pw) < MIN_PASSWORD_LEN) {
        printf("Password must be at least %d characters.\n", MIN_PASSWORD_LEN);
        return 0;
    }
    if (strlen(pw) > 40) { printf("Password is too long (max 40).\n"); return 0; }
    read_password("Confirm password: ", again, sizeof again);
    if (strcmp(pw, again) != 0) { printf("Passwords do not match.\n"); return 0; }
    return 1;
}

/* ------------------------------------------------------------------ public */
void auth_init(void)
{
    List users = fh_load_users();
    srand((unsigned)time(NULL) ^ ((unsigned)clock() << 7));

    if (users.count == 0) {
        User admin;
        memset(&admin, 0, sizeof admin);
        snprintf(admin.username, sizeof admin.username, "%s", DEFAULT_ADMIN_USER);
        admin.role = ROLE_ADMIN;
        set_password(&admin, DEFAULT_ADMIN_PASS);
        if (list_append(&users, &admin, sizeof admin))
            fh_save_users(&users);
    }
    list_clear(&users);
}

int auth_login(void)
{
    int attempt;
    for (attempt = 1; attempt <= MAX_LOGIN_ATTEMPTS; attempt++) {
        char name[64], pw[64];
        List users;
        Node *n;

        read_line("Username: ", name, sizeof name);
        read_password("Password: ", pw, sizeof pw);

        users = fh_load_users();
        n = find_user(&users, name);
        if (n && check_password((User *)n->data, pw)) {
            g_current_user = *(User *)n->data;
            printf("\nWelcome, %s (%s).\n", g_current_user.username, role_name(g_current_user.role));
            if (g_current_user.role == ROLE_ADMIN && check_password(&g_current_user, DEFAULT_ADMIN_PASS))
                printf("Warning: default admin password is still in use. Change it from 'Change My Password'.\n");
            list_clear(&users);
            return 1;
        }
        list_clear(&users);
        printf("Invalid username or password. (%d/%d)\n", attempt, MAX_LOGIN_ATTEMPTS);
    }
    return 0;
}

void change_own_password(void)
{
    char old[64], pw[64];
    List users = fh_load_users();
    Node *n = find_user(&users, g_current_user.username);
    User *u;

    if (!n) { printf("Your account no longer exists.\n"); list_clear(&users); return; }
    u = n->data;

    read_password("Current password: ", old, sizeof old);
    if (!check_password(u, old)) {
        printf("Current password is incorrect.\n"); list_clear(&users); return;
    }
    if (read_new_password(pw, sizeof pw)) {
        set_password(u, pw);
        if (fh_save_users(&users)) { g_current_user = *u; printf("Password changed.\n"); }
    }
    list_clear(&users);
}

/* ---------------------------------------------------------- admin: user mgmt */
static void list_users(void)
{
    List users = fh_load_users();
    Node *n;
    printf("\n%-22s %s\n", "Username", "Role");
    printf("-------------------------------\n");
    for (n = users.head; n; n = n->next) {
        const User *u = n->data;
        printf("%-22s %s\n", u->username, role_name(u->role));
    }
    printf("\nTotal users: %d\n", users.count);
    list_clear(&users);
}

static void add_user(void)
{
    char name[64], pw[64];
    List users;
    User u;

    read_line("New username (3-20 letters/digits/_): ", name, sizeof name);
    if (!valid_username(name)) { printf("Invalid username.\n"); return; }

    users = fh_load_users();
    if (find_user(&users, name)) { printf("Username already exists.\n"); list_clear(&users); return; }
    if (!read_new_password(pw, sizeof pw)) { list_clear(&users); return; }

    printf("Role: 1. Staff   2. Admin\n");
    memset(&u, 0, sizeof u);
    snprintf(u.username, sizeof u.username, "%s", name);
    u.role = (read_int("Choice: ", 1, 2) == 2) ? ROLE_ADMIN : ROLE_STAFF;
    set_password(&u, pw);

    if (!list_append(&users, &u, sizeof u)) { printf("Out of memory.\n"); list_clear(&users); return; }
    if (fh_save_users(&users)) printf("User '%s' created as %s.\n", u.username, role_name(u.role));
    list_clear(&users);
}

static void delete_user(void)
{
    char name[64];
    List users = fh_load_users();
    Node *n;

    read_line("Username to delete: ", name, sizeof name);
    n = find_user(&users, name);
    if (!n) { printf("User not found.\n"); list_clear(&users); return; }
    if (eq_ci(((User *)n->data)->username, g_current_user.username)) {
        printf("You cannot delete your own account.\n"); list_clear(&users); return;
    }
    if (confirm("Delete this user permanently? (y/n): ")) {
        list_remove(&users, n);
        if (fh_save_users(&users)) printf("User deleted.\n");
    } else printf("Cancelled.\n");
    list_clear(&users);
}

static void reset_password(void)
{
    char name[64], pw[64];
    List users = fh_load_users();
    Node *n;

    read_line("Username to reset: ", name, sizeof name);
    n = find_user(&users, name);
    if (!n) { printf("User not found.\n"); list_clear(&users); return; }
    if (read_new_password(pw, sizeof pw)) {
        User *u = n->data;
        set_password(u, pw);
        if (fh_save_users(&users)) printf("Password reset for '%s'.\n", u->username);
    }
    list_clear(&users);
}

void user_management_menu(void)
{
    int c;
    if (!require_role(ROLE_ADMIN)) return;
    do {
        printf("\n===== USER MANAGEMENT =====\n"
               "1. List Users\n2. Add User\n3. Delete User\n4. Reset User Password\n0. Back\n");
        c = read_int("Choice: ", 0, 4);
        switch (c) {
            case 1: list_users();     break;
            case 2: add_user();       break;
            case 3: delete_user();    break;
            case 4: reset_password(); break;
        }
    } while (c != 0);
}