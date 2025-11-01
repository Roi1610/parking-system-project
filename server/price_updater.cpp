#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <limits>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include "sqlite3.h"

/// @brief Path to the prices file.
const std::string PRICES_FILE = "prices.txt";

/// @brief Name of the shared memory segment.
const std::string SHM_NAME = "/prices_shm";

/// @brief Size of the shared memory segment.
const size_t SHM_SIZE = 2048;

/// @brief File containing the server PID.
const std::string SERVER_PID_FILE = "server.pid";

/// @brief SQLite database file name used by the server.
const std::string DB_FILE = "data.db";

/**
 * @brief Writes the prices map into a shared memory segment.
 * 
 * @param prices Map of city_code -> price_per_hour.
 */
void write_prices_to_shm(const std::unordered_map<int,double>& prices) {
    int fd = shm_open(SHM_NAME.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd < 0) { perror("shm_open"); exit(1); }
    if (ftruncate(fd, SHM_SIZE) < 0) { perror("ftruncate"); exit(1); }

    void* ptr = mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) { perror("mmap"); exit(1); }

    size_t count = prices.size();
    memcpy(ptr, &count, sizeof(size_t));
    uint8_t* p = (uint8_t*)ptr + sizeof(size_t);

    for (const auto &kv : prices) {
        memcpy(p, &kv.first, sizeof(int)); p += sizeof(int);
        memcpy(p, &kv.second, sizeof(double)); p += sizeof(double);
    }

    munmap(ptr, SHM_SIZE);
    close(fd);
}


/**
 * @brief Loads prices from PRICES_FILE into a map.
 * 
 * @return std::unordered_map<int,double> Mapping of city_code -> price.
 */
std::unordered_map<int,double> load_prices_file() {
    std::unordered_map<int,double> prices;
    std::ifstream f(PRICES_FILE);
    if(!f.is_open()) return prices;

    std::string line;
    while(std::getline(f,line)){
        std::istringstream ss(line);
        std::string token;
        if(std::getline(ss, token, ',')){
            int code = std::stoi(token);
            if(std::getline(ss, token, ',')){
                double price = std::stod(token);
                prices[code] = price;
            }
        }
    }
    return prices;
}

/**
 * @brief Saves prices from the map into PRICES_FILE.
 * 
 * @param prices Map of city_code -> price_per_hour.
 */
void save_prices_file(const std::unordered_map<int,double>& prices) {
    std::ofstream out(PRICES_FILE, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "[ERROR] Cannot write to " << PRICES_FILE << "\n";
        return;
    }
    for (const auto &kv : prices)
        out << kv.first << "," << kv.second << "\n";
}

/**
 * @brief Updates the SQLite database with the provided prices.
 *        If a city_code does not exist, a new row is inserted.
 * 
 * @param prices Map of city_code -> price_per_hour.
 * @param city_name Optional city name for new inserts.
 * @param lat Optional GPS latitude for new inserts.
 * @param lng Optional GPS longitude for new inserts.
 */
void update_db_from_prices_file(const std::unordered_map<int,double>& prices,
                                const std::string& city_name = "",
                                double lat = 0.0,
                                double lng = 0.0) {
    sqlite3* db;
    if(sqlite3_open(DB_FILE.c_str(), &db) != SQLITE_OK){
        std::cerr << "[SQL-ERR] Cannot open DB: " << sqlite3_errmsg(db) << "\n";
        return;
    }

    char* errmsg = nullptr;
    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, &errmsg);

    for(const auto &kv : prices){
        int code = kv.first;
        double price = kv.second;

        sqlite3_stmt* stmt = nullptr;
        const char* sql_update = "UPDATE prices SET price_per_hour=?1 WHERE city_code=?2;";
        if(sqlite3_prepare_v2(db, sql_update, -1, &stmt, nullptr) != SQLITE_OK){
            std::cerr << "[SQL-ERR] prepare UPDATE: " << sqlite3_errmsg(db) << "\n";
            continue;
        }

        sqlite3_bind_double(stmt, 1, price);
        sqlite3_bind_int(stmt, 2, code);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if(sqlite3_changes(db) == 0){
            // If not exists, perform full INSERT
            sqlite3_stmt* stmt_insert = nullptr;
            const char* sql_insert =
                "INSERT INTO prices(city, city_code, gps_lat, gps_lng, price_per_hour, created_at) "
                "VALUES(?1, ?2, ?3, ?4, ?5, datetime('now'));";
            if(sqlite3_prepare_v2(db, sql_insert, -1, &stmt_insert, nullptr) == SQLITE_OK){
                sqlite3_bind_text(stmt_insert, 1, city_name.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt_insert, 2, code);
                sqlite3_bind_double(stmt_insert, 3, lat);
                sqlite3_bind_double(stmt_insert, 4, lng);
                sqlite3_bind_double(stmt_insert, 5, price);
                if(sqlite3_step(stmt_insert) != SQLITE_DONE){
                    std::cerr << "[SQL-ERR] INSERT failed for city_code=" << code << ": "
                              << sqlite3_errmsg(db) << "\n";
                }
                sqlite3_finalize(stmt_insert);
            } else {
                std::cerr << "[SQL-ERR] prepare INSERT: " << sqlite3_errmsg(db) << "\n";
            }
        }
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errmsg);
    if(errmsg){ std::cerr << "[SQL-ERR] COMMIT: " << errmsg << "\n"; sqlite3_free(errmsg); }

    sqlite3_close(db);
}

/**
 * @brief Sends a SIGHUP signal to the server process to reload prices.
 */
void signal_server() {
    std::ifstream f(SERVER_PID_FILE);
    if(!f.is_open()){ std::cerr << "[ERROR] Cannot open server.pid\n"; return; }
    pid_t pid; f >> pid;
    if(pid <= 0){ std::cerr << "[ERROR] Invalid PID\n"; return; }
    if(kill(pid, SIGHUP) < 0) perror("kill SIGHUP");
    else std::cout << "[INFO] Sent SIGHUP to server (pid=" << pid << ")\n";
}

/**
 * @brief Adds a new city and its price, updates files, SHM, DB and signals server.
 * 
 * @param prices Map of city_code -> price_per_hour.
 */
void add_new_city(std::unordered_map<int,double>& prices){
    std::string city;
    int code;
    double lat, lng, price;

    std::cout << "Enter city name: ";
    std::getline(std::cin, city);
    std::cout << "Enter city code (int): ";
    std::cin >> code;
    std::cout << "Enter GPS latitude: ";
    std::cin >> lat;
    std::cout << "Enter GPS longitude: ";
    std::cin >> lng;
    std::cout << "Enter price per hour: ";
    std::cin >> price;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    prices[code] = price;
    save_prices_file(prices);
    write_prices_to_shm(prices);
    update_db_from_prices_file(prices, city, lat, lng);
    signal_server();
    std::cout << "[INFO] City added successfully.\n";
}

/**
 * @brief Updates the price of an existing city, updates files, SHM, DB and signals server.
 * 
 * @param prices Map of city_code -> price_per_hour.
 */
void update_city_price(std::unordered_map<int,double>& prices){
    std::cout << "--- Current Prices ---\n";
    for(const auto &kv: prices) std::cout << kv.first << " -> " << kv.second << "\n";

    int code; double price;
    std::cout << "Enter city code to update: "; std::cin >> code;
    std::cout << "Enter new price: "; std::cin >> price;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if(prices.find(code)!=prices.end()){
        prices[code] = price;
        save_prices_file(prices);
        write_prices_to_shm(prices);
        update_db_from_prices_file(prices);
        signal_server();
        std::cout << "[INFO] Price updated successfully.\n";
    } else {
        std::cerr << "[ERROR] City code not found.\n";
    }
}

/**
 * @brief Removes a city from memory, files, DB and signals server.
 * 
 * @param prices Map of city_code -> price_per_hour.
 */
void remove_city(std::unordered_map<int,double>& prices){
    int code;
    std::cout << "Enter city code to remove: ";
    std::cin >> code;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    bool found = (prices.erase(code) > 0);

    if(found){
        save_prices_file(prices);
        write_prices_to_shm(prices);

        sqlite3* db;
        if(sqlite3_open(DB_FILE.c_str(), &db) == SQLITE_OK){
            const char* sql_delete = "DELETE FROM prices WHERE city_code = ?1;";
            sqlite3_stmt* stmt = nullptr;

            if(sqlite3_prepare_v2(db, sql_delete, -1, &stmt, nullptr) == SQLITE_OK){
                sqlite3_bind_int(stmt, 1, code);
                if(sqlite3_step(stmt) != SQLITE_DONE){
                    std::cerr << "[SQL-ERR] DELETE failed for city_code=" << code
                              << ": " << sqlite3_errmsg(db) << "\n";
                }
                sqlite3_finalize(stmt);
            } else {
                std::cerr << "[SQL-ERR] prepare DELETE: " << sqlite3_errmsg(db) << "\n";
            }

            sqlite3_close(db);
        } else {
            std::cerr << "[SQL-ERR] Cannot open DB: " << sqlite3_errmsg(db) << "\n";
        }

        signal_server();
        std::cout << "[INFO] City removed successfully.\n";
    } else {
        std::cerr << "[ERROR] City code not found.\n";
    }
}



/**
 * @brief Main entry point for the price updater program.
 *        Provides a menu for updating, adding, or removing cities.
 * 
 * @return int Exit code.
 */
int main(){
    auto prices = load_prices_file();

    std::cout << "\n--- Price Updater ---\n";
    std::cout << "1. Update city price\n";
    std::cout << "2. Add new city\n";
    std::cout << "3. Remove city\n";
    std::cout << "Select option: ";
    int choice; std::cin >> choice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    switch(choice){
        case 1: update_city_price(prices); break;
        case 2: add_new_city(prices); break;
        case 3: remove_city(prices); break;
        default: std::cerr << "[ERROR] Invalid choice.\n"; return 1;
    }

    std::cout << "[DONE] Update complete.\n";
    return 0;
}