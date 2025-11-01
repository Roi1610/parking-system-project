#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <unistd.h>
#include <atomic>
#include "sqlite3.h"

/**
 * @brief RAII wrapper for sqlite3* database handle.
 * Ensures the database connection is closed when the object goes out of scope.
 */
struct DBHandle {
    sqlite3* db = nullptr; /// Pointer to SQLite database

    /// @brief Destructor closes the database if open 
    ~DBHandle() { if(db) sqlite3_close(db); }

    DBHandle() = default;

    /// @brief Move constructor transfers ownership 
    DBHandle(DBHandle&& other) noexcept : db(other.db) { other.db = nullptr; }

    /// @brief Move assignment operator transfers ownership
    DBHandle& operator=(DBHandle&& other) noexcept { 
        if(this != &other) {
            if(db) sqlite3_close(db);
            db = other.db; other.db = nullptr;
        }
        return *this;
    }
    DBHandle(const DBHandle&) = delete;              /// Copy constructor deleted
    DBHandle& operator=(const DBHandle&) = delete;   ///< Copy assignment deleted

    /// @brief Get raw sqlite3 pointer
    sqlite3* get() const { return db; }
};

/**
 * @brief RAII wrapper for sqlite3_stmt* statement handle.
 * Ensures the prepared statement is finalized when the object goes out of scope.
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
 * @brief RAII wrapper for a socket file descriptor.
 * Ensures the socket is closed when the object goes out of scope.
 */
struct SocketRAII {
    int fd = -1;  /// File descriptor

    /// @brief Constructor with optional fd initialization
    SocketRAII(int f = -1) : fd(f) {}

    /// @brief Destructor closes the socket if valid
    ~SocketRAII() { if(fd >= 0) ::close(fd); }

    SocketRAII(const SocketRAII&) = delete;             /// Copy constructor deleted
    SocketRAII& operator=(const SocketRAII&) = delete;  /// Copy assignment deleted

    /// @brief Move constructor transfers ownership
    SocketRAII(SocketRAII&& other) noexcept : fd(other.fd) { other.fd = -1; }

    /// @brief Move assignment operator transfers ownership
    SocketRAII& operator=(SocketRAII&& other) noexcept { 
        if(this != &other) {
            if(fd >= 0) ::close(fd);
            fd = other.fd; other.fd = -1;
        }
        return *this;
    }
};

/**
 * @brief Main server class managing DB, sockets, and requests.
 */
class Server {
public:
    Server();
    ~Server();

    Server(const Server&) = delete;             /// Copy constructor deleted
    Server& operator=(const Server&) = delete;  /// Copy assignment deleted
    Server(Server&&) = default;                 /// Move constructor default
    Server& operator=(Server&&) = default;      /// Move assignment default

    /**
     * @brief Start the server (blocking call).
     * @return 0 on success, non-zero on failure.
     */
    int start();

private:
    DBHandle db_;                 /// RAII SQLite database handle
    StmtHandle stmt_insert_open_; /// Statement handle for insert open

    /// @brief Additional statements for RAII
    StmtHandle stmt_check_open_;
    StmtHandle stmt_find_open_;
    StmtHandle stmt_minutes_;
    StmtHandle stmt_price_;
    StmtHandle stmt_update_close_;
    StmtHandle stmt_find_city_; 

    /** @brief Initialize the database, creating tables if necessary */
    int init_db();

    /** @brief Prepare all required SQLite statements */
    int prepare_statements();

    /** 
     * @brief Main server loop handling client connections.
     * @param listen_fd Listening socket file descriptor
     * @return 0 on success, non-zero on failure
     */
    int run_loop(int listen_fd);

    /**
     * @brief Log formatted messages to stdout or log file.
     * @param fmt printf-style format string
     * @param ... Arguments
     */
    void logf(const char *fmt, ...);
};

#endif // SERVER_H