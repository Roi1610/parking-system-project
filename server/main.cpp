#include "server.h"
#include "config.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <fstream>
#include <unistd.h> 

/**
 * @brief Main entry point for the server application.
 *
 * This program initializes the server, writes its PID to a file,
 * and starts the server in blocking mode. It also handles exceptions
 * thrown during server startup and logs them.
 *
 * @return int Exit code (0 on success, non-zero on failure)
 */
int main()
{
    try {
         /**
         * @brief Create a file named "server.pid" containing the current process ID.
         *
         * This allows external scripts or monitoring tools to know the
         * PID of the running server process.
         */
        {
            std::ofstream pid_file("server.pid", std::ios::trunc);
            if (pid_file.is_open()) {
                pid_file << getpid() << std::endl;
                std::cout << "[INFO] server.pid created with PID " << getpid() << std::endl;
            } else {
                std::cerr << "[WARN] Failed to create server.pid" << std::endl;
            }
        }

         /**
         * @brief Initialize the server object and start it.
         *
         * The server runs in blocking mode until it is stopped or encounters an error.
         */
        Server srv;
        int rc = srv.start();  /// Start the server (blocking call)
        if(rc != 0){
            std::cerr << "[ERROR] Server exited with code " << rc << "\n";
            return rc;
        }
    } catch(const std::exception &e) {
        /**
         * @brief Catch any standard exceptions thrown during startup.
         *
         * Logs the exception message to stderr and exits with -1.
         */
        std::cerr << "[EXCEPTION] " << e.what() << std::endl;
        return -1;
    }
    return 0;    /// Successful execution
}