#include "client.h"

int sock = -1;

int main() {
    pid_t pid = fork();
    if (pid < 0) {
        log_message("Error: Daemon fork failed.");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        log_message("Parent process exiting after daemon fork.");
        exit(EXIT_SUCCESS); // Parent exits
    }

    log_message("Daemon setup.(chiled)");

    /* Daemon setup */
    if (setsid() == -1){
        log_message("create new session failed");
        exit(EXIT_FAILURE);
    }

    /* Full socket permission */
    umask(0); 

    /* set the working directory to the root directory */
    if (chdir("/") == -1){
        log_message("failed to enter root directory.");
        exit(EXIT_FAILURE);
    }

    // /**
    //  * @brief Close the standard input, output, and error file descriptors.
    //  *
    //  * This is typically done in daemon processes to detach from any terminal
    //  * and prevent the daemon from accidentally reading from or writing to
    //  * the controlling terminal.
    //  * Closing these descriptors helps ensure the daemon runs silently in the
    //  * background and avoids unintended interactions with user input/output.
    //  */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* handle signals */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);
    signal(SIGQUIT, handle_signal);

    /* Declare socket address structure for client connection. */
    struct sockaddr_in client_name;

    /* Declare gps_frame structure to hold received GPS data. */
    gps_frame frame;

     log_message("start socket....");

    /* Create a TCP socket. */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_message("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    /**
    * @brief Establish connection to the server using the socket.
    * @param sock Socket file descriptor.
    * @param client_name Pointer to sockaddr_in structure to configure server address.
    */
    connect_to_server(sock, &client_name);

    /* Create PIPE FIFO if not exists*/
    if(access(PIPE_PATH, F_OK) != 0){
        if(mkfifo(PIPE_PATH, 0666) < 0){
            if(errno != EEXIST){
                log_message("Failed to create PIPE");
                exit(EXIT_FAILURE);
            }
        else{
            log_message("PIPE created by client");
        }
        }
    }

    /* Open PIPE FIFO for reading*/
    int pipe_fd = open(PIPE_PATH, O_RDONLY);
    if (pipe_fd < 0){
        log_message("Failed to open PIPE for reading");
        exit(EXIT_FAILURE);
    }
    log_message("PIPE opend for reading");

    /* Polling setup for both pipe and socket */
    struct pollfd fds[2];
    fds[0].fd = pipe_fd;
    fds[0].events = POLLIN;
    fds[1].fd = sock;
    fds[1].events = POLLIN;

    /**
    * @brief Main loop to wait for data on read_fd and send it to the server.
    * @details Uses poll to block indefinitely (-1) until data is available on read_fd.
    *          When data is ready, it reads a gps_frame and sends it over the socket.
    *          If poll returns an error, logs the error and breaks the loop.
    */
    while (1) {
        int ret = poll(fds, 2, -1); // block indefinitely
        if (ret < 0) {
            log_message("Poll error. Exiting client.");
            close(sock);
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }

        /* Data available from PIPE */
        if (fds[0].revents & POLLIN) {
            get_frame(&frame, pipe_fd);
            send_data(sock, &frame);
        }

        /* Socket disconnected or error */
        if (fds[1].revents & (POLLHUP | POLLERR)) {
            log_message("Server closed connection. Exiting client.");
            close(sock);
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }

    }

    /* Close the socket file descriptor when done. */
    close(sock);
    close(pipe_fd);
    return 0;
}