#ifndef CLIENT_H
#define CLIENT_H

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"
#include <stdbool.h>
#include <poll.h>
#include <fcntl.h>

//#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_ADDRESS "10.100.102.30"
#define PORT 13777
#define LOG_FILE "/home/debian/embedded/parking_client.log"
#define PIPE_PATH "/tmp/i2c_pipe"

extern int sock;

/**
 * @brief write log result to file
 * @param message - 
 * @param timestamp - date and time the message was sent
 */
static void log_message(const char *message){
    FILE *log = fopen(LOG_FILE, "a");
    if(!log){
        perror("Failed to open file");
        return;
    }
    time_t now=time(NULL);
    char ts[64];
    strftime(ts,sizeof(ts),"%d-%m-%Y %H:%M:%S",localtime(&now));
    fprintf(log,"[%s] %s\n\n",ts,message);
    fclose(log);
}


/**
 * @brief handle signals
 */
static void handle_signal(int signal){
    switch (signal)
    {
    case SIGINT:
        log_message("Caught SIGINT, Closing socket");
        if (sock != -1){
            close(sock);
        }
        exit(EXIT_FAILURE);
        break;

    case SIGTERM:
        log_message("Caught SIGTERM, Closing socket");
        if (sock != -1){
            close(sock);
        }
        exit(EXIT_FAILURE);
        break;

    case SIGHUP:
        log_message("Caught SIGHUP, Closing socket");
        if (sock != -1){
            close(sock);
        }
        exit(EXIT_FAILURE);
        break;

    case SIGQUIT:
        log_message("Caught SIGQUIT, Closing socket");
        if (sock != -1){
            close(sock);
        }
        exit(EXIT_FAILURE);
        break;

    default:
        log_message("Caught signal, Closing socket");
        if (sock != -1){
            close(sock);
        }
        exit(EXIT_FAILURE);
        break;
    }
}


static void connect_to_server(int sock, struct sockaddr_in *client_name){
    log_message("Client is alive and establishing socket connection.");
    bzero(client_name, sizeof(*client_name));
    client_name->sin_family = AF_INET;
    client_name->sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    client_name->sin_port = htons(PORT);


    if (connect(sock, (struct sockaddr *)client_name, sizeof(*client_name)) < 0)
    {
        log_message("Error establishing communications");
        close(sock);
        exit(EXIT_FAILURE);
    }

    log_message("Connected to server");
}

static void get_frame(gps_frame *from_stm, int read_fd)
{
  ssize_t bytes = read(read_fd, from_stm, sizeof(gps_frame));
  if (bytes != sizeof(gps_frame)){
    log_message("Failed to read full gps_frame from pipe");
  }
  else{
    log_message("Frame received from I2C via pipe");
  }
}

// send data to server
static void send_data(int sock, gps_frame *from_stm){
    // Direct sending (frame already in network endian from i2c_daemon)
    ssize_t sent = write(sock, from_stm, sizeof(*from_stm));
    if (sent < 0)
    {
        log_message("Error writing to socket");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Convert only the 16-bit fields back to host order
    uint16_t dev_id = ntohs(from_stm->device_id);
    uint16_t status = ntohs(from_stm->status);

    // Keep float as-is (no conversion)
    float x = from_stm->cord_x;
    float y = from_stm->cord_y;

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "Frame sent to server: ID=%u, X=%.3f, Y=%.3f, STATUS=%u",
             dev_id, x, y, status);
    log_message(buffer);
}


#endif

