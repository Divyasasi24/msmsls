/*
 * test_medsupply.cpp  -  GoogleTest suite for the Medical Supply Management POC
 *
 * Covers every non-static function in:
 *   linkedlist.c, utils.c, filehandler.c, auth.c, inventory.c, stock.c, requests.c, distribution.c
 * (and, optionally, main.c - see below).
 *
 * HOW THE INTERACTIVE FUNCTIONS ARE TESTED
 *   - stdin  is replaced with a temp file  (SetInput / RunWith helpers)
 *   - stdout is captured with testing::internal::CaptureStdout()
 *   - functions that call exit(0) on EOF are tested with EXPECT_EXIT (death tests)
 *   - every test starts and ends with the *.dat files deleted (fixture)
 *   - dates are generated relative to "today", so tests never go stale
 *
 * BUILD (Linux / MinGW, with coverage)
 *   gcc -c --coverage -Wall auth.c distribution.c filehandler.c inventory.c \
 *                     linkedlist.c requests.c stock.c utils.c
 *   g++ -std=c++17 --coverage test_medsupply.cpp *.o -lgtest -lpthread -o run_tests
 *   ./run_tests
 *   gcovr -r . --html-details -o coverage.html        (or lcov / gcov)
 *
 *   NOTE: this file has its own main(), so main.c must NOT be linked as-is.
 *
 * OPTIONAL - also cover main.c (admin_menu / staff_menu / main):
 *   gcc -c --coverage -Dmain=app_main main.c -o main_app.o
 *   g++ -std=c++17 --coverage -DCOVER_MAIN_C test_medsupply.cpp *.o -lgtest -lpthread -o run_tests
 *
 * If your gtest is older than 1.10, replace INSTANTIATE_TEST_SUITE_P with
 * INSTANTIATE_TEST_CASE_P.
 */
#include <gtest/gtest.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#ifdef _WIN32
  #include <direct.h>
  #define MAKE_DIR(p)   _mkdir(p)
  #define REMOVE_DIR(p) _rmdir(p)
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <unistd.h>
  #define MAKE_DIR(p)   mkdir((p), 0755)
  #define REMOVE_DIR(p) rmdir(p)
#endif

extern "C"
{
#include "medsupply.h"
}

#ifdef COVER_MAIN_C
extern "C" int app_main(void);   /* main.c compiled with -Dmain=app_main */
#endif

/*=====================================================
  Test helpers
=====================================================*/
static const char *kStdinFile = "gts_stdin.tmp";
static const char *kDataFiles[] = { ITEMS_FILE, REQUESTS_FILE, HISTORY_FILE, USERS_FILE };

#define EXPECT_HAS(text, needle) \
    EXPECT_NE(std::string::npos, (text).find(needle)) \
        << "Expected to find: " << (needle) << "\n--- output ---\n" << (text)
#define EXPECT_LACKS(text, needle) \
    EXPECT_EQ(std::string::npos, (text).find(needle)) \
        << "Did not expect: " << (needle) << "\n--- output ---\n" << (text)

/* Joins lines with '\n' (each line terminated) - simulates typed input. */
static std::string Lines(const std::vector<std::string> &v)
{
    std::string s;
    for (size_t i = 0; i < v.size(); i++) { s += v[i]; s += '\n'; }
    return s;
}

/* Replace stdin with the given text. */
static void SetInput(const std::string &text)
{
    FILE *f = fopen(kStdinFile, "wb");
    if (f) { fwrite(text.data(), 1, text.size(), f); fclose(f); }
    FILE *r = freopen(kStdinFile, "r", stdin);
    (void)r;
}

/* Feed `input` to stdin, run fn(), return everything fn() printed. */
template <typename Fn>
static std::string RunWith(const std::string &input, Fn fn)
{
    SetInput(input);
    testing::internal::CaptureStdout();
    fn();
    return testing::internal::GetCapturedStdout();
}

/* "YYYY-MM-DD" for today + `days` (negative = past). */
static std::string DateOffset(int days)
{
    time_t now = time(NULL);
    struct tm t = *localtime(&now);
    t.tm_mday += days;
    t.tm_hour = 12; t.tm_min = 0; t.tm_sec = 0; t.tm_isdst = -1;
    mktime(&t);
    char buf[16];
    strftime(buf, sizeof buf, "%Y-%m-%d", &t);
    return buf;
}

static void SetCurrentUser(const char *name, int role)
{
    memset(&g_current_user, 0, sizeof g_current_user);
    snprintf(g_current_user.username, sizeof g_current_user.username, "%s", name);
    g_current_user.role = role;
}

/* ---- record builders ---- */
static Item MakeItem(int id, const char *name, const char *cat, int qty,
                     float price, const std::string &expiry, int minLevel)
{
    Item it;
    memset(&it, 0, sizeof it);
    it.id = id;
    snprintf(it.name, sizeof it.name, "%s", name);
    snprintf(it.category, sizeof it.category, "%s", cat);
    it.quantity = qty;
    it.price = price;
    snprintf(it.expiry, sizeof it.expiry, "%s", expiry.c_str());
    it.min_level = minLevel;
    return it;
}

static Request MakeReq(int id, int itemId, const char *who, int qty, int status,
                       const char *remarks = "", const char *by = "staff1")
{
    Request r;
    memset(&r, 0, sizeof r);
    r.req_id = id;
    r.item_id = itemId;
    snprintf(r.requester, sizeof r.requester, "%s", who);
    r.qty = qty;
    r.status = status;
    snprintf(r.date, sizeof r.date, "%s", "2026-01-01");
    snprintf(r.remarks, sizeof r.remarks, "%s", remarks);
    snprintf(r.created_by, sizeof r.created_by, "%s", by);
    return r;
}

static Distribution MakeDist(int distId, int reqId, int itemId, const char *itemName,
                             const char *recipient, int qty, const char *by = "staff1")
{
    Distribution d;
    memset(&d, 0, sizeof d);
    d.dist_id = distId;
    d.req_id = reqId;
    d.item_id = itemId;
    snprintf(d.item_name, sizeof d.item_name, "%s", itemName);
    snprintf(d.recipient, sizeof d.recipient, "%s", recipient);
    d.qty = qty;
    snprintf(d.date, sizeof d.date, "%s", "2026-01-01");
    snprintf(d.issued_by, sizeof d.issued_by, "%s", by);
    return d;
}

/* ---- list <-> array/vector bridges (tests build data as arrays, code uses lists) ---- */
template <typename T>
static List ToList(const T *a, int n)
{
    List l; list_init(&l);
    for (int i = 0; i < n; i++) list_append(&l, &a[i], sizeof(T));
    return l;
}
template <typename T>
static std::vector<T> FromList(List &l)           /* copies the records, then frees the list */
{
    std::vector<T> v;
    for (Node *n = l.head; n; n = n->next) v.push_back(*(T *)n->data);
    list_clear(&l);
    return v;
}
/* Array-style wrappers around the list-based save functions. */
static int SaveItemsArr(const Item *a, int n)       { List l = ToList(a, n); int rc = fh_save_items(&l);    list_clear(&l); return rc; }
static int SaveRequestsArr(const Request *a, int n) { List l = ToList(a, n); int rc = fh_save_requests(&l); list_clear(&l); return rc; }
static int SaveUsersArr(const User *a, int n)       { List l = ToList(a, n); int rc = fh_save_users(&l);    list_clear(&l); return rc; }
/* Index of the item with `id` in an array, found through find_item() on a list (-1 = not found). */
static int FindIdx(const Item *a, int n, int id)
{
    List l = ToList(a, n);
    Node *hit = find_item(&l, id);
    int idx = -1, i = 0;
    for (Node *p = l.head; p; p = p->next, i++) if (p == hit) idx = i;
    list_clear(&l);
    return idx;
}
static int ReservedArr(const Request *a, int n, int item_id)
{
    List l = ToList(a, n);
    int r = reserved_qty(&l, item_id);
    list_clear(&l);
    return r;
}

/* ---- seed / load helpers ---- */
static void SeedItems(const std::vector<Item> &v)
{
    ASSERT_EQ(1, SaveItemsArr(v.empty() ? NULL : &v[0], (int)v.size()));
}
static void SeedRequests(const std::vector<Request> &v)
{
    ASSERT_EQ(1, SaveRequestsArr(v.empty() ? NULL : &v[0], (int)v.size()));
}
static void SeedHistory(const std::vector<Distribution> &v)
{
    for (size_t i = 0; i < v.size(); i++) ASSERT_EQ(1, fh_append_history(&v[i]));
}

static std::vector<Item>         LoadItems()    { List l = fh_load_items();    return FromList<Item>(l); }
static std::vector<Request>      LoadRequests() { List l = fh_load_requests(); return FromList<Request>(l); }
static std::vector<User>         LoadUsers()    { List l = fh_load_users();    return FromList<User>(l); }
static std::vector<Distribution> LoadHistory()  { List l = fh_load_history();  return FromList<Distribution>(l); }

static void WriteRaw(const char *path, const char *mode, size_t nBytes)
{
    FILE *f = fopen(path, mode);
    if (f) { std::string s(nBytes, 'x'); fwrite(s.data(), 1, s.size(), f); fclose(f); }
}

/* ---- auth helpers ---- */
static void InitAuth()
{
    fh_init_files();
    auth_init();                       /* creates admin/admin123 */
}

/* Creates a user by driving the admin "Add User" menu. */
static void AddUserViaMenu(const char *name, const char *pw, int role)
{
    InitAuth();
    User saved = g_current_user;
    SetCurrentUser("admin", ROLE_ADMIN);
    RunWith(Lines({ "2", name, pw, pw, role == ROLE_ADMIN ? "2" : "1", "0" }),
            [] { user_management_menu(); });
    g_current_user = saved;
}

static int Login(const char *user, const char *pw, std::string *outText = NULL)
{
    int ok = -1;
    std::string out = RunWith(Lines({ user, pw }), [&] { ok = auth_login(); });
    if (outText) *outText = out;
    return ok;
}

/*=====================================================
  Fixtures
=====================================================*/
class MedSupplyTest : public ::testing::Test
{
protected:
    static void CleanFiles()
    {
        for (size_t i = 0; i < sizeof(kDataFiles) / sizeof(kDataFiles[0]); i++)
        {
            remove(kDataFiles[i]);
            REMOVE_DIR(kDataFiles[i]);      /* in case a test made it a directory */
        }
    }
    void SetUp() override
    {
        CleanFiles();
        memset(&g_current_user, 0, sizeof g_current_user);
    }
    void TearDown() override
    {
        CleanFiles();
        remove(kStdinFile);
    }
};

class UtilsTest        : public MedSupplyTest {};
class FileHandlerTest  : public MedSupplyTest {};
class AuthTest         : public MedSupplyTest {};
class InventoryTest    : public MedSupplyTest {};
class StockTest        : public MedSupplyTest {};
class RequestsTest     : public MedSupplyTest {};
class DistributionTest : public MedSupplyTest {};
class InputEofDeathTest: public MedSupplyTest {};
#ifdef COVER_MAIN_C
class MainMenuTest     : public MedSupplyTest {};
#endif

/*#####################################################
  utils.c
#####################################################*/

/*=====================================================
  read_line()
=====================================================*/
TEST_F(UtilsTest, ReadLine_StripsNewlineAndPrintsPrompt)
{
    char buf[32];
    std::string out = RunWith("hello\n", [&] { read_line("Name: ", buf, sizeof buf); });
    EXPECT_STREQ("hello", buf);
    EXPECT_HAS(out, "Name: ");
}

TEST_F(UtilsTest, ReadLine_EmptyLine)
{
    char buf[32] = "junk";
    RunWith("\n", [&] { read_line("p", buf, sizeof buf); });
    EXPECT_STREQ("", buf);
}

TEST_F(UtilsTest, ReadLine_NullPromptPrintsNothing)
{
    char buf[32];
    std::string out = RunWith("abc\n", [&] { read_line(NULL, buf, sizeof buf); });
    EXPECT_STREQ("abc", buf);
    EXPECT_EQ("", out);
}

TEST_F(UtilsTest, ReadLine_LongInputTruncatedAndRestFlushed)
{
    char a[8], b[8];
    RunWith("ABCDEFGHIJKLMNOP\nnext\n", [&] {
        read_line(NULL, a, sizeof a);
        read_line(NULL, b, sizeof b);
    });
    EXPECT_STREQ("ABCDEFG", a);
    EXPECT_STREQ("next", b);
}

TEST_F(UtilsTest, ReadLine_ExactFitInputDoesNotLeakNewlineIntoNextRead)
{
    char a[8], b[8];
    RunWith("ABCDEFG\nnext\n", [&] {
        read_line(NULL, a, sizeof a);
        read_line(NULL, b, sizeof b);
    });
    EXPECT_STREQ("ABCDEFG", a);
    EXPECT_STREQ("next", b);
}

TEST_F(UtilsTest, ReadLine_LastLineWithoutNewline)
{
    char buf[16];
    RunWith("abc", [&] { read_line(NULL, buf, sizeof buf); });
    EXPECT_STREQ("abc", buf);
}

/*=====================================================
  read_int()
=====================================================*/
TEST_F(UtilsTest, ReadInt_ValidNumber)
{
    int v = 0;
    RunWith("42\n", [&] { v = read_int("N: ", 0, 100); });
    EXPECT_EQ(42, v);
}

TEST_F(UtilsTest, ReadInt_MinBoundaryAccepted)
{
    int v = -1;
    RunWith("5\n", [&] { v = read_int("N: ", 5, 10); });
    EXPECT_EQ(5, v);
}

TEST_F(UtilsTest, ReadInt_MaxBoundaryAccepted)
{
    int v = -1;
    RunWith("10\n", [&] { v = read_int("N: ", 5, 10); });
    EXPECT_EQ(10, v);
}

TEST_F(UtilsTest, ReadInt_BelowMinReprompts)
{
    int v = 0;
    std::string out = RunWith("4\n7\n", [&] { v = read_int("N: ", 5, 10); });
    EXPECT_EQ(7, v);
    EXPECT_HAS(out, "Invalid input. Enter a number between 5 and 10.");
}

TEST_F(UtilsTest, ReadInt_AboveMaxReprompts)
{
    int v = 0;
    std::string out = RunWith("11\n10\n", [&] { v = read_int("N: ", 5, 10); });
    EXPECT_EQ(10, v);
    EXPECT_HAS(out, "Invalid input");
}

TEST_F(UtilsTest, ReadInt_NonNumericReprompts)
{
    int v = 0;
    std::string out = RunWith("abc\n3\n", [&] { v = read_int("N: ", 0, 5); });
    EXPECT_EQ(3, v);
    EXPECT_HAS(out, "Invalid input");
}

TEST_F(UtilsTest, ReadInt_TrailingGarbageRejected)
{
    int v = 0;
    std::string out = RunWith("5x\n6\n", [&] { v = read_int("N: ", 0, 10); });
    EXPECT_EQ(6, v);
    EXPECT_HAS(out, "Invalid input");
}

TEST_F(UtilsTest, ReadInt_EmptyLineRejected)
{
    int v = 0;
    std::string out = RunWith("\n2\n", [&] { v = read_int("N: ", 0, 10); });
    EXPECT_EQ(2, v);
    EXPECT_HAS(out, "Invalid input");
}

TEST_F(UtilsTest, ReadInt_NegativeRange)
{
    int v = 0;
    RunWith("-3\n", [&] { v = read_int("N: ", -5, 5); });
    EXPECT_EQ(-3, v);
}

TEST_F(UtilsTest, ReadInt_LeadingWhitespaceAndPlusSignAccepted)
{
    int a = 0, b = 0;
    RunWith("  7\n+8\n", [&] { a = read_int("N: ", 0, 10); b = read_int("N: ", 0, 10); });
    EXPECT_EQ(7, a);
    EXPECT_EQ(8, b);
}

TEST_F(UtilsTest, ReadInt_TrailingSpaceRejected)
{
    int v = 0;
    std::string out = RunWith("7 \n8\n", [&] { v = read_int("N: ", 0, 10); });
    EXPECT_EQ(8, v);
    EXPECT_HAS(out, "Invalid input");
}

TEST_F(UtilsTest, ReadInt_HugeNumberRejected)
{
    int v = 0;
    std::string out = RunWith("99999999999999999999\n1\n", [&] { v = read_int("N: ", 0, 10); });
    EXPECT_EQ(1, v);
    EXPECT_HAS(out, "Invalid input");
}

TEST_F(UtilsTest, ReadInt_HexPrefixRejected)
{
    int v = 0;
    std::string out = RunWith("0x10\n4\n", [&] { v = read_int("N: ", 0, 100); });
    EXPECT_EQ(4, v);
    EXPECT_HAS(out, "Invalid input");
}

/*=====================================================
  read_float()
=====================================================*/
TEST_F(UtilsTest, ReadFloat_ValidDecimal)
{
    float v = 0;
    RunWith("3.5\n", [&] { v = read_float("F: ", 0.0f, 10.0f); });
    EXPECT_FLOAT_EQ(3.5f, v);
}

TEST_F(UtilsTest, ReadFloat_IntegerTextAccepted)
{
    float v = 0;
    RunWith("7\n", [&] { v = read_float("F: ", 0.0f, 10.0f); });
    EXPECT_FLOAT_EQ(7.0f, v);
}

TEST_F(UtilsTest, ReadFloat_ScientificNotationAccepted)
{
    float v = 0;
    RunWith("1e2\n", [&] { v = read_float("F: ", 0.0f, 1000.0f); });
    EXPECT_FLOAT_EQ(100.0f, v);
}

TEST_F(UtilsTest, ReadFloat_BoundariesAccepted)
{
    float lo = -1, hi = -1;
    RunWith("0\n10\n", [&] { lo = read_float("F: ", 0.0f, 10.0f); hi = read_float("F: ", 0.0f, 10.0f); });
    EXPECT_FLOAT_EQ(0.0f, lo);
    EXPECT_FLOAT_EQ(10.0f, hi);
}

TEST_F(UtilsTest, ReadFloat_BelowMinReprompts)
{
    float v = 0;
    std::string out = RunWith("-1\n2.5\n", [&] { v = read_float("F: ", 0.0f, 10.0f); });
    EXPECT_FLOAT_EQ(2.5f, v);
    EXPECT_HAS(out, "Invalid input. Enter a value between 0.00 and 10.00.");
}

TEST_F(UtilsTest, ReadFloat_AboveMaxReprompts)
{
    float v = 0;
    std::string out = RunWith("10.01\n9\n", [&] { v = read_float("F: ", 0.0f, 10.0f); });
    EXPECT_FLOAT_EQ(9.0f, v);
    EXPECT_HAS(out, "Invalid input");
}

TEST_F(UtilsTest, ReadFloat_NonNumericAndEmptyAndTrailingGarbageRejected)
{
    float v = 0;
    std::string out = RunWith("abc\n\n2.5kg\n4.25\n", [&] { v = read_float("F: ", 0.0f, 10.0f); });
    EXPECT_FLOAT_EQ(4.25f, v);
    EXPECT_HAS(out, "Invalid input");
}

TEST_F(UtilsTest, ReadFloat_NanAndInfRejected)
{
    float v = 0;
    std::string out = RunWith("nan\ninf\n1\n", [&] { v = read_float("F: ", 0.0f, 10.0f); });
    EXPECT_FLOAT_EQ(1.0f, v);
    EXPECT_HAS(out, "Invalid input");
}

/*=====================================================
  confirm()
=====================================================*/
TEST_F(UtilsTest, Confirm_LowerCaseYes)
{
    int r = 0;
    RunWith("y\n", [&] { r = confirm("ok? "); });
    EXPECT_EQ(1, r);
}

TEST_F(UtilsTest, Confirm_UpperCaseYes)
{
    int r = 0;
    RunWith("Y\n", [&] { r = confirm("ok? "); });
    EXPECT_EQ(1, r);
}

TEST_F(UtilsTest, Confirm_WordStartingWithYIsYes)
{
    int r = 0;
    RunWith("yes\n", [&] { r = confirm("ok? "); });
    EXPECT_EQ(1, r);
}

TEST_F(UtilsTest, Confirm_NoVariantsReturnZero)
{
    int a = -1, b = -1, c = -1, d = -1;
    RunWith("n\nN\nmaybe\n\n", [&] {
        a = confirm("? "); b = confirm("? "); c = confirm("? "); d = confirm("? ");
    });
    EXPECT_EQ(0, a);
    EXPECT_EQ(0, b);
    EXPECT_EQ(0, c);
    EXPECT_EQ(0, d);
}

/*=====================================================
  get_today()
=====================================================*/
TEST_F(UtilsTest, GetToday_HasIsoFormat)
{
    char today[11];
    get_today(today);
    ASSERT_EQ(10u, strlen(today));
    EXPECT_EQ('-', today[4]);
    EXPECT_EQ('-', today[7]);
    EXPECT_EQ(1, valid_date(today));
}

TEST_F(UtilsTest, GetToday_MatchesSystemDate)
{
    char today[11];
    get_today(today);
    EXPECT_EQ(DateOffset(0), std::string(today));
}

/*=====================================================
  valid_date()   (data driven)
=====================================================*/
struct DateCase { const char *text; int expected; };

class ValidDateParamTest : public ::testing::TestWithParam<DateCase> {};

TEST_P(ValidDateParamTest, Classifies)
{
    EXPECT_EQ(GetParam().expected, valid_date(GetParam().text)) << "input: " << GetParam().text;
}

INSTANTIATE_TEST_SUITE_P(Dates, ValidDateParamTest, ::testing::Values(
    DateCase{"2027-12-31", 1},
    DateCase{"2000-01-01", 1},      /* lowest year        */
    DateCase{"2100-12-31", 1},      /* highest year       */
    DateCase{"2026-06-15", 1},
    DateCase{"1999-12-31", 0},      /* year too low       */
    DateCase{"2101-01-01", 0},      /* year too high      */
    DateCase{"2027-00-10", 0},      /* month 0            */
    DateCase{"2027-13-10", 0},      /* month 13           */
    DateCase{"2027-15-31", 0},
    DateCase{"2027-12-00", 0},      /* day 0              */
    DateCase{"2027-12-32", 0},      /* day 32             */
    DateCase{"31-12-2027", 0},      /* wrong order        */
    DateCase{"2027/12/31", 0},      /* wrong separator    */
    DateCase{"2027-1-31",  0},      /* too short          */
    DateCase{"2027-12-311", 0},     /* too long           */
    DateCase{"abcd-ef-gh", 0},      /* non digits         */
    DateCase{"2027-1a-31", 0},      /* letter inside      */
    DateCase{"",           0},      /* empty              */
    DateCase{"2027-02-31", 1}       /* current behaviour: no per-month day check */
));

/*=====================================================
  days_until()
=====================================================*/
TEST_F(UtilsTest, DaysUntil_TodayIsZero)       { EXPECT_EQ(0,   days_until(DateOffset(0).c_str())); }
TEST_F(UtilsTest, DaysUntil_TomorrowIsOne)     { EXPECT_EQ(1,   days_until(DateOffset(1).c_str())); }
TEST_F(UtilsTest, DaysUntil_YesterdayIsMinus1) { EXPECT_EQ(-1,  days_until(DateOffset(-1).c_str())); }
TEST_F(UtilsTest, DaysUntil_ThirtyDaysAhead)   { EXPECT_EQ(30,  days_until(DateOffset(30).c_str())); }
TEST_F(UtilsTest, DaysUntil_ThirtyDaysAgo)     { EXPECT_EQ(-30, days_until(DateOffset(-30).c_str())); }
TEST_F(UtilsTest, DaysUntil_AcrossDstBoundary) { EXPECT_EQ(180, days_until(DateOffset(180).c_str())); }
TEST_F(UtilsTest, DaysUntil_OneYearAhead)      { EXPECT_EQ(365, days_until(DateOffset(365).c_str())); }
TEST_F(UtilsTest, DaysUntil_FixedFarFuture)    { EXPECT_GT(days_until("2099-01-01"), 0); }
TEST_F(UtilsTest