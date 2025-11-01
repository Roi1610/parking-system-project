#ifndef UTILS_H
#define UTILS_H

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include "sqlite3.h"
#include <string>
#include <ctime>


/**
 * @brief Safely finalize a SQLite statement and set the pointer to NULL.
 * 
 * @param pstmt Pointer to the SQLite statement to finalize.
 */
#define SAFE_FINALIZE(pstmt)           \
    do                                 \
    {                                  \
        if ((pstmt))                   \
        {                              \
            sqlite3_finalize((pstmt)); \
            (pstmt) = NULL;            \
        }                              \
    } while (0)


/**
 * @brief Checks the result of a SQLite operation and exits on error.
 * 
 * @param rc Return code from SQLite function.
 * @param db Pointer to the SQLite database.
 * @param msg Message to display on error.
 * 
 * If the return code indicates an error, prints the message and SQLite error,
 * waits for Enter key, closes the database, and exits the program.
 */
#define CHECK_SQL(rc, db, msg)                                                                 \
    do                                                                                         \
    {                                                                                          \
        if ((rc) != SQLITE_OK && (rc) != SQLITE_DONE && (rc) != SQLITE_ROW)                    \
        {                                                                                      \
            fprintf(stderr, "[SQL-ERR] %s: rc=%d, msg=%s\n", (msg), (rc), sqlite3_errmsg(db)); \
            fprintf(stderr, "Press Enter to exit...");                                         \
            (void)getchar();                                                                   \
            if (db)                                                                            \
                sqlite3_close(db);                                                             \
            exit(1);                                                                           \
        }                                                                                      \
    } while (0)

namespace utils
{
    /**
     * @brief Extracts a string value from a JSON object.
     * 
     * @param json JSON string to parse.
     * @param key Key of the value to extract.
     * @param out Buffer to store the extracted string.
     * @param out_sz Size of the output buffer.
     * @return true if the key was found and value copied, false otherwise.
     */
    bool json_get_string(const char *json, const char *key, char *out, std::size_t out_sz);

    /**
     * @brief Extracts a double value from a JSON object.
     * 
     * @param json JSON string to parse.
     * @param key Key of the value to extract.
     * @param out Pointer to store the extracted double.
     * @return true if the key was found and value extracted, false otherwise.
     */
    bool json_get_double(const char *json, const char *key, double *out);

    /**
     * @brief Extracts a long value from a JSON object.
     * 
     * @param json JSON string to parse.
     * @param key Key of the value to extract.
     * @param out Pointer to store the extracted long.
     * @return true if the key was found and value extracted, false otherwise.
     */
    bool json_get_long(const char *json, const char *key, long *out);

    // Initialize DB schema and seed price table (creates 'prices' with city column)
    // Returns 0 on success, non-zero on error.
    int init_db_schema_and_seed(sqlite3 *db);

    // Return current local time in YYYY-MM-DD HH:MM:SS.sss format
    std::string current_local_time();
}

#endif // UTILS_H