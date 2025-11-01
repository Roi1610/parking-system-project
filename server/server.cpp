#include "server.h"
#include "config.h"
#include "utils.h"
#include "protocol.h"
#include <sstream> 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <poll.h>
#include <netinet/tcp.h>
#include <ctime>
#include <fstream>
#include <atomic>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unordered_map>

/// @brief Shared memory name for inter-process price updates.
const std::string SHM_NAME = "/prices_shm";


/// @brief Shared memory size (enough for dozens of prices).
const size_t SHM_SIZE = 1024; 

/// @brief Local prices file path.
const std::string PRICES_FILE = "prices.txt";

/// @brief In-memory cache of prices by city_code.
std::unordered_map<int,double> prices_cache; // מחיר לכל city_code

// --------------------------------------------------------------------------------
/**
 * @class SignalHandlerRAII
 * @brief RAII-style signal handler manager.
 * 
 * Handles system signals such as SIGINT, SIGTERM, SIGQUIT, and SIGHUP.
 * Allows safe shutdown and dynamic price update signaling.
 */
class SignalHandlerRAII {
public:
    struct SigGuard {
        static std::atomic<bool> stop;
        static std::atomic<int> sig_received;
        static std::atomic<bool> update_prices;

        static void handle(int sig) {
            if(sig == SIGHUP){
                update_prices.store(true);
            } else {
                stop.store(true);
                sig_received.store(sig);
            }
        }

        SigGuard() {
            struct sigaction sa{};
            sa.sa_handler = handle;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;

            sigaction(SIGINT, &sa, nullptr);
            sigaction(SIGTERM, &sa, nullptr);
            sigaction(SIGQUIT, &sa, nullptr);
            sigaction(SIGHUP, &sa, nullptr);
        }
    };

    /// @brief Constructor automatically initializes the guard.
    SignalHandlerRAII() { (void)guard; }

    /// @brief Get last received signal.
    /// @return The signal number.
    static int get_signal() {
        return SigGuard::sig_received.load();
    }

    /// @brief Check if a price update (SIGHUP) was requested.
    static bool need_update_prices() {
        return SigGuard::update_prices.load();
    }

    /// @brief Reset the SIGHUP update flag.
    static void reset_update_flag() {
        SigGuard::update_prices.store(false);
    }

private:
    inline static SigGuard guard{};
};

std::atomic<bool> SignalHandlerRAII::SigGuard::stop{false};
std::atomic<int> SignalHandlerRAII::SigGuard::sig_received{-1};
std::atomic<bool> SignalHandlerRAII::SigGuard::update_prices{false};

// --------------------------------------------------------------------------------
/**
 * @brief Convert a 32-bit float from big-endian (network order) to host order.
 * @param f_net The float in network order.
 * @return The float in host order.
 */
static float float_from_big_endian(float f_net)
{
    union {
        float f;
        uint32_t i;
    } u;
    u.f = f_net;
    u.i = ntohl(u.i);
    return u.f;
}

/**
 * @brief Round a double value to three decimal places.
 * @param val The input value.
 * @return Rounded value.
 */
static double round3(double val)
{
    return std::round(val * 1000.0) / 1000.0;
}

// --------------------------------------------------------------------------------
/**
 * @brief Write the current prices table from the database into prices.txt.
 * @param db SQLite database handle.
 */
static void write_prices_file_from_db(sqlite3* db)
{
    const char* sql_select_prices = "SELECT city_code, price_per_hour FROM prices;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql_select_prices, -1, &stmt, nullptr);
    if(rc != SQLITE_OK){
        std::cerr << "[SQL-ERR] Failed to prepare select prices: " << sqlite3_errmsg(db) << "\n";
        return;
    }

    std::ofstream f(PRICES_FILE, std::ios::trunc);
    if(!f.is_open()){
        std::cerr << "[ERR] Cannot open prices file for writing: " << PRICES_FILE << "\n";
        sqlite3_finalize(stmt);
        return;
    }

    while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
        int city_code = sqlite3_column_int(stmt, 0);
        double price = sqlite3_column_double(stmt, 1);
        f << city_code << "," << price << "\n";
    }
    sqlite3_finalize(stmt);
    f.close();

    std::cout << "[INFO] Prices file generated: " << PRICES_FILE << "\n";
}

// --------------------------------------------------------------------------------
/**
 * @brief Loads price data from shared memory. 
 * If the shared memory does not exist, it will be created and initialized as empty.
 */
static void load_prices_from_shm()
{
    int fd = shm_open(SHM_NAME.c_str(), O_RDONLY, 0);
    if (fd < 0) {
        fd = shm_open(SHM_NAME.c_str(), O_CREAT | O_RDWR, 0666);
        if (fd < 0) {
            perror("shm_open create");
            return;
        }

        if (ftruncate(fd, SHM_SIZE) < 0) {
            perror("ftruncate");
            close(fd);
            return;
        }

        void* ptr = mmap(nullptr, SHM_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            perror("mmap init");
            close(fd);
            return;
        }

        size_t count = 0;
        memcpy(ptr, &count, sizeof(size_t));
        munmap(ptr, SHM_SIZE);
        close(fd);

        fprintf(stderr, "[WARN] Shared memory not found — created new empty segment '%s'\n", SHM_NAME.c_str());
        return; 
    }

    void* ptr = mmap(nullptr, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return;
    }

    prices_cache.clear();
    size_t count = 0;
    memcpy(&count, ptr, sizeof(size_t));

    if (count > 0) {
        uint8_t* p = (uint8_t*)ptr + sizeof(size_t);
        for (size_t i = 0; i < count; i++) {
            int code;
            double price;
            memcpy(&code, p, sizeof(int)); p += sizeof(int);
            memcpy(&price, p, sizeof(double)); p += sizeof(double);
            prices_cache[code] = price;
        }
    }

    munmap(ptr, SHM_SIZE);
    close(fd);
}

// --------------------------------------------------------------------------------
/**
 * @brief Update the prices table in the database from the local prices.txt file.
 * @param db SQLite database handle.
 * @param cache In-memory cache to update simultaneously.
 */
static void update_db_from_prices_file(sqlite3* db, std::unordered_map<int,double>& cache)
{
    std::ifstream f(PRICES_FILE);
    if(!f.is_open()) {
        std::cerr << "[ERR] Cannot open prices file for reading: " << PRICES_FILE << "\n";
        return;
    }

    char* errmsg = nullptr;
    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, &errmsg);
    if(errmsg) { std::cerr << "[SQL-ERR] BEGIN transaction: " << errmsg << "\n"; sqlite3_free(errmsg); }

    std::string line;
    while(std::getline(f, line)){
        if(line.empty()) continue;
        std::istringstream ss(line);
        int code; double price;
        char comma;
        ss >> code >> comma >> price;
        if(ss.fail()) continue;

        // --------- Try UPDATE first ---------
        sqlite3_stmt* stmt_update = nullptr;
        const char* sql_update = "UPDATE prices SET price_per_hour=?1 WHERE city_code=?2;";
        int rc = sqlite3_prepare_v2(db, sql_update, -1, &stmt_update, nullptr);
        if(rc != SQLITE_OK){
            std::cerr << "[SQL-ERR] Prepare UPDATE for city_code=" << code << ": " << sqlite3_errmsg(db) << "\n";
            continue;
        }

        sqlite3_bind_double(stmt_update, 1, price);
        sqlite3_bind_int(stmt_update, 2, code);
        rc = sqlite3_step(stmt_update);
        sqlite3_finalize(stmt_update);

        if(sqlite3_changes(db) == 0) {
            sqlite3_stmt* stmt_insert = nullptr;
            const char* sql_insert = "INSERT INTO prices(city_code, price_per_hour) VALUES(?1, ?2);";
            rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt_insert, nullptr);
            if(rc != SQLITE_OK){
                std::cerr << "[SQL-ERR] Prepare INSERT for city_code=" << code << ": " << sqlite3_errmsg(db) << "\n";
                continue;
            }

            sqlite3_bind_int(stmt_insert, 1, code);
            sqlite3_bind_double(stmt_insert, 2, price);
            rc = sqlite3_step(stmt_insert);
            if(rc != SQLITE_DONE){
                std::cerr << "[SQL-ERR] INSERT step failed for city_code=" << code << ": " << sqlite3_errmsg(db) << "\n";
            }
            sqlite3_finalize(stmt_insert);
        }

        // Update cache
        cache[code] = price;
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errmsg);
    if(errmsg) { std::cerr << "[SQL-ERR] COMMIT transaction: " << errmsg << "\n"; sqlite3_free(errmsg); }

    std::cout << "[INFO] Database updated from " << PRICES_FILE << "\n";
}


// --------------------------------------------------------------------------------
/**
 * @brief Default constructor.
 */
Server::Server() = default;

/**
 * @brief Destructor that ensures RAII cleanup for database and prepared statements.
 */
Server::~Server() = default; 

/**
 * @brief Write formatted log message to stdout and log file.
 * @param fmt printf-style format string.
 */
void Server::logf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char mbuf[2048];
    vsnprintf(mbuf, sizeof(mbuf), fmt, ap);
    va_end(ap);

    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char tbuf[64];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm_buf);

    std::cout << "[" << tbuf << "] " << mbuf << std::endl;

    std::ofstream f(SERVER_LOG, std::ios::app);
    if(f.is_open()) {
        f << "[" << tbuf << "] " << mbuf << "\n";
    }
}

/**
 * @brief Initialize SQLite database connection and schema.
 * @return SQLITE_OK on success, otherwise error code.
 */
int Server::init_db()
{
    DBHandle tmp_db;
    int rc = sqlite3_open(DB_FILE, &tmp_db.db);
    if(rc != SQLITE_OK || !tmp_db.db) {
        logf("[SQL-ERR] sqlite3_open failed: %s",
            tmp_db.db ? sqlite3_errmsg(tmp_db.db) : "sqlite3_open returned nullptr");
        return rc != SQLITE_OK ? rc : -1;
    }

    db_ = std::move(tmp_db);

    logf("[INIT] SQLite runtime version: %s", sqlite3_libversion());
    rc = utils::init_db_schema_and_seed(db_.db);
    if(rc != 0) {
        logf("[SQL-ERR] init_db_schema_and_seed failed");
        return rc;
    }

    // Generate prices.txt automatically
    write_prices_file_from_db(db_.db);

    return SQLITE_OK;
}

/**
 * @brief Prepare all required SQL statements for efficient reuse.
 * 
 * This function prepares all SQLite statements the server will use frequently,
 * storing them in member variables for fast execution. Statements include insert,
 * update, and select operations for customer and price data.
 * 
 * @return int SQLITE_OK on success, otherwise the SQLite error code.
 */
int Server::prepare_statements()
{
    const char *sql_insert_open =
        "INSERT INTO customer_data "
        "(customer_id, city_code, gps_lat, gps_lng, status , parking_duration_minutes, ticket_fee, created_at)"
        "VALUES (?1, ?2, ?3, ?4, 1, 0, 0.0, ?5);";

    const char *sql_check_open =
        "SELECT rowid FROM customer_data WHERE customer_id=?1 AND city_code=?2 "
        "AND ABS(gps_lat - ?3)<0.0001 AND ABS(gps_lng - ?4)<0.0001 AND status=1 LIMIT 1;";

    const char *sql_find_open =
        "SELECT rowid, created_at FROM customer_data WHERE customer_id=?1 AND city_code=?2 "
        "AND ABS(gps_lat - ?3)<0.0001 AND ABS(gps_lng - ?4)<0.0001 AND status=1 "
        "ORDER BY created_at DESC LIMIT 1;";

    const char *sql_minutes =
        "SELECT CAST((strftime('%s','now','localtime') - strftime('%s', ?1)) / 60 AS INTEGER);";

    const char *sql_price =
        "SELECT price_per_hour FROM prices WHERE city_code=?1 LIMIT 1;";

    const char *sql_update_close =
        "UPDATE customer_data SET status=0, parking_duration_minutes=?1, ticket_fee=?2, ended_at=?3 "
        "WHERE rowid=?4;";

    const char *sql_find_city =
        "SELECT city_code FROM prices WHERE ABS(gps_lat - ?1)<0.0001 "
        "AND ABS(gps_lng - ?2)<0.0001 LIMIT 1;";

    int rc;
    rc = sqlite3_prepare_v2(db_.db, sql_insert_open, -1, &stmt_insert_open_.stmt, nullptr);
    CHECK_SQL(rc, db_.db, "prepare insert open");

    rc = sqlite3_prepare_v2(db_.db, sql_check_open, -1, &stmt_check_open_.stmt, nullptr);
    CHECK_SQL(rc, db_.db, "prepare check open");

    rc = sqlite3_prepare_v2(db_.db, sql_find_open, -1, &stmt_find_open_.stmt, nullptr);
    CHECK_SQL(rc, db_.db, "prepare find open");

    rc = sqlite3_prepare_v2(db_.db, sql_minutes, -1, &stmt_minutes_.stmt, nullptr);
    CHECK_SQL(rc, db_.db, "prepare minutes");

    rc = sqlite3_prepare_v2(db_.db, sql_price, -1, &stmt_price_.stmt, nullptr);
    CHECK_SQL(rc, db_.db, "prepare price");

    rc = sqlite3_prepare_v2(db_.db, sql_update_close, -1, &stmt_update_close_.stmt, nullptr);
    CHECK_SQL(rc, db_.db, "prepare update close");

    rc = sqlite3_prepare_v2(db_.db, sql_find_city, -1, &stmt_find_city_.stmt, nullptr);
    CHECK_SQL(rc, db_.db, "prepare find city");

    return SQLITE_OK;
}

/**
 * @brief Read exactly n bytes from non-blocking socket with signal handling.
 * @param fd File descriptor.
 * @param buf Buffer to read into.
 * @param n Number of bytes to read.
 * @return Number of bytes read, -1 on error, -2 if SIGHUP/update-prices requested.
 */
static ssize_t read_n_nonblocking(int fd, void *buf, size_t n)
{
    size_t got = 0;
    char *p = (char*)buf;

    while(got < n) {
        
        if(SignalHandlerRAII::SigGuard::stop.load()) return -1;  // stop requested
        if(SignalHandlerRAII::need_update_prices()) return -2;   // price update requested

        ssize_t r = recv(fd, p + got, n - got, 0);
        if(r > 0) {
            got += (size_t)r;
            continue;
        }
        if(r == 0) return (ssize_t)got;

        // retry on signal interrupt
        if(errno == EINTR) {
            
            continue;
        }
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            for(int i=0;i<5;i++){
                if(SignalHandlerRAII::need_update_prices()) return -2;
                usleep(2000); // 10ms total sleep
            }
            continue;
        }
        return -1;  // other errors
    }
    return (ssize_t)got;
}

/**
 * @brief Main server loop to accept clients and process their GPS frames.
 * 
 * The server listens for client connections, sets non-blocking mode, applies
 * TCP keepalive options, and processes GPS frames in a loop. It also handles
 * SIGHUP updates and termination signals.
 * 
 * @param listen_fd Listening socket file descriptor.
 */
int Server::run_loop(int listen_fd)
{
    SocketRAII listen_sock(listen_fd);
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    SignalHandlerRAII signal_guard; 

    int flags = fcntl(listen_sock.fd, F_GETFL, 0);
    fcntl(listen_sock.fd, F_SETFL, flags | O_NONBLOCK);

    // Load prices initially from shared memory
    load_prices_from_shm();

    /**
    * @brief Main server loop handling incoming client connections and processing GPS frames.
    * This loop continuously accepts new clients on the listening socket, sets
    * appropriate socket options, and processes GPS frame data. It also checks
    * for SIGHUP signals to update prices in real-time.
    */
    while (!SignalHandlerRAII::SigGuard::stop.load()) {

        /**
        * @brief Check if a price update has been requested via SIGHUP.
        * If the update flag is set, refresh the database from the prices file
        * and reload the shared memory cache. Reset the update flag afterward.
        */
        if(SignalHandlerRAII::need_update_prices()){
            logf("[INFO] SIGHUP received: updating prices from file and shared memory...");
            update_db_from_prices_file(db_.db, prices_cache);
            load_prices_from_shm();
            logf("[INFO] Prices update completed.");
            SignalHandlerRAII::reset_update_flag();
        }

        /** 
        * @brief Accept a new client connection.
        * 
        * Resets client_len for safety and accepts a client on the listening socket.
         */
        client_len = sizeof(client_addr);
        int client_fd = accept(listen_sock.fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if(errno==EINTR) continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK) { usleep(10000); continue; }
            logf("[SOCK-ERR] accept() failed: %s", strerror(errno));
            continue;
        }

        /// @brief Wrap the client socket in RAII and configure it for non-blocking I/O.
        SocketRAII client_sock(client_fd);
        int cflags = fcntl(client_sock.fd, F_GETFL, 0);
        fcntl(client_sock.fd, F_SETFL, cflags | O_NONBLOCK);


        /// @brief Enable TCP keep-alive to detect dead peers and avoid stale connections.
        int yes = 1;
        setsockopt(client_sock.fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
        int idle=10, interval=5, count=3;
        setsockopt(client_sock.fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
        setsockopt(client_sock.fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
        setsockopt(client_sock.fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));

        /// @brief Convert client IP and port to human-readable format and log connection.
        char ipbuf[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf));
        int client_port = ntohs(client_addr.sin_port);
        logf("[INFO] Client connected from %s:%d (fd=%d)", ipbuf, client_port, client_sock.fd);

        /// @brief Inner loop to handle client-specific GPS frames until disconnect or stop.
        while (!SignalHandlerRAII::SigGuard::stop.load()) {

            /// @brief Real-time check for SIGHUP to update prices while client connected.
            if(SignalHandlerRAII::need_update_prices()){
                logf("[INFO] SIGHUP received: updating prices from file and shared memory...");
                update_db_from_prices_file(db_.db, prices_cache);
                load_prices_from_shm();
                logf("[INFO] Prices update completed.");
                SignalHandlerRAII::reset_update_flag();
            }

            /**
            * @brief Read a GPS frame from the non-blocking client socket.
            * 
            * Handles special return values:
            * - -2: price update requested
            * - <=0: client disconnected or error
            */
            gps_frame raw;
            ssize_t n = read_n_nonblocking(client_sock.fd, &raw, sizeof(raw));

            if(n == -2) {
                logf("[INFO] Detected price-update request while client connected. Applying update...");
                update_db_from_prices_file(db_.db, prices_cache);
                load_prices_from_shm();
                logf("[INFO] Prices update completed while client connected.");
                SignalHandlerRAII::reset_update_flag();
                // continue to wait for client data
                continue;
            }

            if(n <= 0) {
                if(n < 0 && (errno != ECONNRESET && errno != EPIPE))
                    logf("[SOCK-ERR] recv error from %s:%d: %s", ipbuf, client_port, strerror(errno));
                else
                    logf("[INFO] Client disconnected from %s:%d (fd=%d)", ipbuf, client_port, client_sock.fd);
                break;
            }
       
            /// @brief Parse GPS frame and convert coordinates/status to usable format.
            uint16_t dev_id = ntohs(raw.device_id);
            uint16_t status = ntohs(raw.status);
            double x = round3(float_from_big_endian(raw.cord_x));
            double y = round3(float_from_big_endian(raw.cord_y));

            logf("[RECV] From %s:%d -> ID=%u, X=%.3f, Y=%.3f, STATUS=%u",
                 ipbuf, client_port, (unsigned)dev_id, x, y, (unsigned)status);

            /// @brief Generate string ID for customer based on device ID.
            char customer_id[64];
            snprintf(customer_id, sizeof(customer_id), "%u", (unsigned)dev_id);

            /// @brief Determine city code for the GPS coordinates using prepared statement.
            int city_code = 0;
            sqlite3_reset(stmt_find_city_.stmt);
            sqlite3_clear_bindings(stmt_find_city_.stmt);
            sqlite3_bind_double(stmt_find_city_.stmt, 1, x);
            sqlite3_bind_double(stmt_find_city_.stmt, 2, y);
            int rc = sqlite3_step(stmt_find_city_.stmt);
            if(rc == SQLITE_ROW) city_code = sqlite3_column_int(stmt_find_city_.stmt, 0);

            /// @brief Handle parking open (status=1) events.
            if(status == 1) {
                /// @brief Check if a parking session is already open for this customer/location.
                sqlite3_reset(stmt_check_open_.stmt);
                sqlite3_clear_bindings(stmt_check_open_.stmt);
                sqlite3_bind_text(stmt_check_open_.stmt, 1, customer_id, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt_check_open_.stmt, 2, city_code);
                sqlite3_bind_double(stmt_check_open_.stmt, 3, x);
                sqlite3_bind_double(stmt_check_open_.stmt, 4, y);

                rc = sqlite3_step(stmt_check_open_.stmt);
                bool already_open = (rc == SQLITE_ROW);

                /// @brief Insert a new parking session if none exists.
                if(!already_open) {
                    sqlite3_reset(stmt_insert_open_.stmt);
                    sqlite3_clear_bindings(stmt_insert_open_.stmt);
                    std::string now_str = utils::current_local_time();
                    sqlite3_bind_text(stmt_insert_open_.stmt, 1, customer_id, -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(stmt_insert_open_.stmt, 2, city_code);
                    sqlite3_bind_double(stmt_insert_open_.stmt, 3, x);
                    sqlite3_bind_double(stmt_insert_open_.stmt, 4, y);
                    sqlite3_bind_text(stmt_insert_open_.stmt, 5, now_str.c_str(), -1, SQLITE_TRANSIENT);
                    rc = sqlite3_step(stmt_insert_open_.stmt);
                    CHECK_SQL(rc, db_.db, "insert raw open step");
                    logf("[DB] Inserted RAW OPEN for customer=%s", customer_id);
                } else {
                    logf("[DB] Already open record exists for customer=%s at coords %.3f,%.3f",
                         customer_id, x, y);
                }
            /// @brief Handle parking close (status=0) events.
            } else if(status == 0) {
                /// @brief Handle closing of an existing parking session.
                sqlite3_reset(stmt_find_open_.stmt);
                sqlite3_clear_bindings(stmt_find_open_.stmt);
                sqlite3_bind_text(stmt_find_open_.stmt, 1, customer_id, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt_find_open_.stmt, 2, city_code);
                sqlite3_bind_double(stmt_find_open_.stmt, 3, x);
                sqlite3_bind_double(stmt_find_open_.stmt, 4, y);

                rc = sqlite3_step(stmt_find_open_.stmt);
                if(rc == SQLITE_ROW) {
                    int rowid = sqlite3_column_int(stmt_find_open_.stmt, 0);
                    const unsigned char *created_at_text = sqlite3_column_text(stmt_find_open_.stmt, 1);

                    /// @brief Calculate parking duration in minutes from the created_at timestamp.
                    sqlite3_reset(stmt_minutes_.stmt);
                    sqlite3_clear_bindings(stmt_minutes_.stmt);
                    sqlite3_bind_text(stmt_minutes_.stmt, 1, (const char*)created_at_text, -1, SQLITE_TRANSIENT);
                    rc = sqlite3_step(stmt_minutes_.stmt);
                    int parking_minutes = 0;
                    if(rc == SQLITE_ROW) parking_minutes = sqlite3_column_int(stmt_minutes_.stmt, 0);

                    /// @brief Lookup the hourly price from cache or database.
                    sqlite3_reset(stmt_price_.stmt);
                    sqlite3_clear_bindings(stmt_price_.stmt);
                    sqlite3_bind_int(stmt_price_.stmt, 1, city_code);
                    rc = sqlite3_step(stmt_price_.stmt);
                    double price_per_hour = (rc==SQLITE_ROW) ? sqlite3_column_double(stmt_price_.stmt,0) : 0.0;

                    if(prices_cache.find(city_code) != prices_cache.end()){
                        price_per_hour = prices_cache[city_code];
                    }

                    /// @brief Calculate the ticket fee and update the database.
                    double ticket_fee = std::round(price_per_hour * parking_minutes / 60.0 * 100.0) / 100.0;

                    sqlite3_reset(stmt_update_close_.stmt);
                    sqlite3_clear_bindings(stmt_update_close_.stmt);
                    std::string ended_at_str = utils::current_local_time();
                    sqlite3_bind_int(stmt_update_close_.stmt, 1, parking_minutes);
                    sqlite3_bind_double(stmt_update_close_.stmt, 2, ticket_fee);
                    sqlite3_bind_text(stmt_update_close_.stmt, 3, ended_at_str.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(stmt_update_close_.stmt, 4, rowid);
                    rc = sqlite3_step(stmt_update_close_.stmt);
                    CHECK_SQL(rc, db_.db, "update close step");

                    logf("[DB] CLOSED customer=%s minutes=%d fee=%.2f",
                         customer_id, parking_minutes, ticket_fee);
                    const char *ok = "OK CLOSED\n";
                    send(client_sock.fd, ok, (int)strlen(ok), 0);
                } else {
                    logf("[DB] No open record found to close for customer=%s at coords %.3f,%.3f",
                         customer_id, x, y);
                }
            }
        }
        /// @brief Client has disconnected, log info.
        logf("[INFO] Client disconnected from %s:%d (fd=%d)", ipbuf, client_port, client_sock.fd);
    }

    if(SignalHandlerRAII::SigGuard::stop.load()) {
        int sig = SignalHandlerRAII::get_signal();
        const char* sig_name = "UNKNOWN";
        switch(sig) {
            case SIGINT: sig_name="SIGINT"; break;
            case SIGTERM: sig_name="SIGTERM"; break;
            case SIGQUIT: sig_name="SIGQUIT"; break;
        }
        logf("[INFO] Terminating due to signal %s", sig_name);
    }

    logf("[INFO] All resources cleaned up, server exiting.");
    return 0;
}


/**
 * @brief Start the server by initializing database, preparing statements, and listening on TCP socket.
 * 
 * This function creates the listening socket, sets SO_REUSEADDR, binds to SERVER_PORT,
 * and starts the main loop for client handling.
 * 
 * @return int SQLITE_OK on success, -1 on socket errors.
 */
int Server::start()
{
    int rc = init_db();
    if(rc != SQLITE_OK) return rc;
    rc = prepare_statements();
    if(rc != SQLITE_OK) return rc;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd < 0) {
        logf("[SOCK-ERR] socket() failed: %s", strerror(errno));
        return -1;
    }

    SocketRAII listen_sock(listen_fd);
    int yes=1;
    setsockopt(listen_sock.fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if(bind(listen_sock.fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        logf("[SOCK-ERR] bind() failed: %s", strerror(errno));
        return -1;
    }

    if(listen(listen_sock.fd, 16) < 0) {
        logf("[SOCK-ERR] listen() failed: %s", strerror(errno));
        return -1;
    }

    logf("[OK] Server listening on port %d...", SERVER_PORT);
    return run_loop(listen_sock.fd);
}