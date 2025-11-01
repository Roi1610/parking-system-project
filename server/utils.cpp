#include "utils.h"

#include <cstring>
#include <cctype>
#include <string>
#include <cstdio>
#include <ctime>

/**
 * @brief RAII wrapper for sqlite3_stmt* (statement handle).
 * Ensures that sqlite3_finalize is called automatically.
 */
struct StmtHandle {
    sqlite3_stmt* stmt = nullptr;  /// Pointer to SQLite statement

    /// @brief Destructor finalizes the statement if not null
    ~StmtHandle() { if(stmt) sqlite3_finalize(stmt); }

    StmtHandle() = default;

    /// @brief Move constructor transfers ownership
    StmtHandle(StmtHandle&& other) noexcept : stmt(other.stmt) { other.stmt = nullptr; }

    /// @brief Move assignment operator transfers ownership
    StmtHandle& operator=(StmtHandle&& other) noexcept {
        if(this != &other) {
            if(stmt) sqlite3_finalize(stmt);
            stmt = other.stmt; other.stmt = nullptr;
        }
        return *this;
    }
    StmtHandle(const StmtHandle&) = delete;             /// Copy constructor deleted
    StmtHandle& operator=(const StmtHandle&) = delete;  /// Copy assignment deleted

    /// @brief Get raw sqlite3_stmt pointer
    sqlite3_stmt* get() const { return stmt; }
};

/**
 * @brief RAII wrapper for sqlite3_exec error message.
 * Ensures that sqlite3_free is called automatically.
 */
struct SqliteErrMsg {
    char* errmsg = nullptr;   /// Pointer to SQLite error message

    /// @brief Default constructor
    SqliteErrMsg() : errmsg(nullptr) {}

    /// @brief Move constructor transfers ownership
    SqliteErrMsg(SqliteErrMsg&& other) noexcept : errmsg(other.errmsg) {
        other.errmsg = nullptr;
    }

    /// @brief Destructor frees the error message if not null
    ~SqliteErrMsg() {
        if (errmsg) sqlite3_free(errmsg);
    }

    SqliteErrMsg(const SqliteErrMsg&) = delete;              /// Copy constructor deleted
    SqliteErrMsg& operator=(const SqliteErrMsg&) = delete;   /// Copy assignment deleted

    /// @brief Move assignment operator transfers ownership
    SqliteErrMsg& operator=(SqliteErrMsg&& other) noexcept {
        if(this != &other) {
            if(errmsg) sqlite3_free(errmsg);
            errmsg = other.errmsg;
            other.errmsg = nullptr;
        }
        return *this;
    }

    /// @brief Get pointer to error message pointer for sqlite3_exec
    char** ptr() { return &errmsg; }
};

namespace
{
    /**
     * @brief Skip whitespace characters in a string.
     * @param s Pointer to the string
     * @return Pointer to the first non-whitespace character
     */
    const char *skip_ws(const char *s)
    {
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
            ++s;
        return s;
    }

    /**
     * @brief Find a JSON key in a JSON string and return pointer to value.
     * @param json JSON string
     * @param key Key to search for
     * @return Pointer to the value after ':' or nullptr if not found
     */
    const char *find_key(const char *json, const char *key)
    {
        char pat[128];
        std::snprintf(pat, sizeof(pat), "\"%s\"", key);
        const char *p = std::strstr(json, pat);
        if (!p)
            return nullptr;
        p += std::strlen(pat);
        p = std::strchr(p, ':');
        return p ? p + 1 : nullptr;
    }
} 

namespace utils
{

/**
 * @brief Extract string value from JSON by key.
 * @param json JSON string
 * @param key Key to search
 * @param out Output buffer
 * @param out_sz Output buffer size
 * @return true if key found and value extracted, false otherwise
 */
bool json_get_string(const char *json, const char *key, char *out, std::size_t out_sz)
{
    const char *p = find_key(json, key);
    if (!p) return false;
    p = skip_ws(p);
    if (*p != '"') return false;
    ++p;
    std::size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_sz)
        out[i++] = *p++;
    if (*p != '"') return false;
    out[i] = '\0';
    return true;
}

/**
 * @brief Extract double value from JSON by key.
 * @param json JSON string
 * @param key Key to search
 * @param out Pointer to double to store result
 * @return true if key found and value extracted, false otherwise
 */
bool json_get_double(const char *json, const char *key, double *out)
{
    const char *p = find_key(json, key);
    if (!p) return false;
    p = skip_ws(p);
    char *endptr = nullptr;
    double v = std::strtod(p, &endptr);
    if (p == endptr) return false;
    *out = v;
    return true;
}

/**
 * @brief Extract long value from JSON by key.
 * @param json JSON string
 * @param key Key to search
 * @param out Pointer to long to store result
 * @return true if key found and value extracted, false otherwise
 */
bool json_get_long(const char *json, const char *key, long *out)
{
    const char *p = find_key(json, key);
    if (!p) return false;
    p = skip_ws(p);
    char *endptr = nullptr;
    long v = std::strtol(p, &endptr, 10);
    if (p == endptr) return false;
    *out = v;
    return true;
}

/**
 * @brief Get current local time as string in format "YYYY-MM-DD HH:MM:SS.sss".
 * @return Time string
 */
std::string current_local_time()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm_buf;
    localtime_r(&ts.tv_sec, &tm_buf);
    char buf[64];
    int ms = (int)(ts.tv_nsec / 1000000);
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        tm_buf.tm_year+1900, tm_buf.tm_mon+1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, ms);
    return std::string(buf);
}

/**
 * @brief Initializes the database schema and seeds the prices table.
 * @param db SQLite database handle
 * @return 0 on success
 */
int init_db_schema_and_seed(sqlite3 *db)
{
    int rc = 0;
    SqliteErrMsg errmsg;

    const char *sql_prices_create =
        "CREATE TABLE IF NOT EXISTS prices ("
        "  city TEXT NOT NULL,"
        "  city_code INTEGER,"
        "  gps_lat REAL,"
        "  gps_lng REAL,"
        "  price_per_hour REAL,"
        "  created_at DATETIME"
        ");";
    rc = sqlite3_exec(db, sql_prices_create, nullptr, nullptr, errmsg.ptr());
    CHECK_SQL(rc, db, "create prices");

    const char *sql_customer_create =
        "CREATE TABLE IF NOT EXISTS customer_data ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  customer_id TEXT,"
        "  city_code INTEGER,"
        "  gps_lat REAL,"
        "  gps_lng REAL,"
        "  status INTEGER,"
        "  parking_duration_minutes INTEGER,"
        "  ticket_fee REAL,"
        "  created_at DATETIME,"
        "  ended_at DATETIME"
        ");";
    rc = sqlite3_exec(db, sql_customer_create, nullptr, nullptr, errmsg.ptr());
    CHECK_SQL(rc, db, "create customer_data");

    struct City{ const char *name; int code; double lat; double lng; double price; };
    City cities[] = {
        {"Rishon Lezion", 8300, 31.962, 34.802, 5.0},
        {"Tel Aviv",      5000, 32.087, 34.789, 5.0},
        {"Jerusalem",     3000, 31.749, 35.170, 7.0},
        {"Eilat",         2600, 29.549, 34.954, 10.0},
        {"Dimona",        2200, 31.073, 35.044, 6.0},
        {"Nahariyya",     9100, 32.999, 35.091, 9.0},
        {"Qiryat Shemona",2800, 33.174, 35.574, 4.0},
        {"Hadera",        6500, 32.422, 34.909, 5.0},
        {"Rehovot",       8400, 31.883, 34.794, 8.0},
        {"Arad",          2560, 31.255, 35.166, 6.0},
    };

    const char *find_sql = "SELECT COUNT(*) FROM prices WHERE ABS(gps_lat - ?1) < 0.0001 AND ABS(gps_lng - ?2) < 0.0001;";
    const char *insert_sql = "INSERT INTO prices (city,city_code,gps_lat,gps_lng,price_per_hour,created_at) VALUES (?1,?2,?3,?4,?5,?6);";

    for(size_t i=0;i<sizeof(cities)/sizeof(cities[0]);++i){
        StmtHandle find;
        rc = sqlite3_prepare_v2(db, find_sql, -1, &find.stmt, nullptr);
        CHECK_SQL(rc, db, "prepare find city");
        rc = sqlite3_bind_double(find.stmt, 1, cities[i].lat);
        CHECK_SQL(rc, db, "bind lat find");
        rc = sqlite3_bind_double(find.stmt, 2, cities[i].lng);
        CHECK_SQL(rc, db, "bind lng find");

        rc = sqlite3_step(find.stmt);
        int cnt=0;
        if(rc==SQLITE_ROW) cnt = sqlite3_column_int(find.stmt,0);

        if(cnt==0){
            StmtHandle ins;
            rc = sqlite3_prepare_v2(db, insert_sql, -1, &ins.stmt, nullptr);
            CHECK_SQL(rc, db, "prepare insert price");
            rc = sqlite3_bind_text(ins.stmt, 1, cities[i].name, -1, SQLITE_TRANSIENT);
            CHECK_SQL(rc, db, "bind name insert");
            rc = sqlite3_bind_int(ins.stmt, 2, cities[i].code);
            CHECK_SQL(rc, db, "bind city_code insert");
            rc = sqlite3_bind_double(ins.stmt, 3, cities[i].lat);
            CHECK_SQL(rc, db, "bind lat insert");
            rc = sqlite3_bind_double(ins.stmt, 4, cities[i].lng);
            CHECK_SQL(rc, db, "bind lng insert");
            rc = sqlite3_bind_double(ins.stmt, 5, cities[i].price);
            CHECK_SQL(rc, db, "bind price insert");
            std::string now_str = current_local_time();
            rc = sqlite3_bind_text(ins.stmt, 6, now_str.c_str(), -1, SQLITE_TRANSIENT);
            CHECK_SQL(rc, db, "bind created_at insert");
            rc = sqlite3_step(ins.stmt);
            CHECK_SQL(rc, db, "step insert price");
        }
    }

    return 0;
}

} // namespace utils