#include "i2c_master.h"
#include "protocol.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>

/**
 * @brief Converts the current process into a daemon.
 * 
 * This function performs the standard steps to daemonize a process:
 * 1. Forks the process and terminates the parent so the child runs in the background.
 * 2. Creates a new session with setsid() to detach from the controlling terminal.
 * 3. Sets file mode creation mask to 0 and changes the working directory to root.
 * 4. Closes standard input, output, and error file descriptors.
 * 
 * After calling this function, the process runs as a daemon, fully detached
 * from any terminal and capable of running in the background.
 */
static void daemonize(void){
    pid_t pid = fork();
    if (pid < 0) {
        log_message("Error: Daemon fork failed.");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        log_message("Parent process exiting after fork.");
        exit(EXIT_SUCCESS); // Parent exits
    }

    if (setsid() == -1){
        log_message("create new session failed");
        exit(EXIT_FAILURE);
    }

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDONLY);  // stdin
    open("/dev/null", O_WRONLY);  // stdout
    open("/dev/null", O_WRONLY);  // stderr

    log_message("Daemonize completed successfully");
}

int pipe_fd = -1;   // global for signal handler
int i2c_fd = -1;
int gpio_fd = -1;

int main(void){
    char logbuf[256];

    /* handle signals */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);
    signal(SIGQUIT, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    /* Daemon setup */
    daemonize();
    log_message("Daemon started");

    /* Make dir for log if not exsist */
    struct stat st = {0};
    if(stat("/home/debian/embedded", &st) == -1){
        mkdir("/home/debian/embedded", 0777);
    }

    /* Create named pipe if not exists */
    if(access(PIPE_PATH, F_OK) != 0){
        if(mkfifo(PIPE_PATH, 0666) < 0){
            log_message("Failed to create PIPE");
        } else {
            log_message("PIPE created");
        }
    }

    /* Open FIFO for writing */
    pipe_fd = open(PIPE_PATH, O_WRONLY | O_NONBLOCK);
    if(pipe_fd < 0){
        log_message("Failed to open PIPE for writing");
    }
    log_message("PIPE opened successfully");

    /* Open I2C bus */
    i2c_fd = open(I2C_BUS, O_RDWR);
    if(i2c_fd < 0){
        log_message("Failed to open I2C bus");
        return 1;
    }
    log_message("I2C bus opened successfully");

    if(ioctl(i2c_fd, I2C_SLAVE, I2C_SLAVE_ADDR) < 0){
        log_message("Failed to set I2C slave address");
        close(i2c_fd);
        return 1;
    }
    log_message("I2C slave address set successfully");

    /* Open GPIO */
    gpio_fd = gpio_init(GPIO_NUM);
    if(gpio_fd < 0){
        log_message("GPIO initialization failed, exiting");
        close(i2c_fd);
        return 1;
    }
    log_message("GPIO initialized successfully");

    struct pollfd pfd;
    pfd.fd = gpio_fd;
    pfd.events = POLLPRI | POLLERR;

    uint16_t id, status;
    float x, y;
    gps_frame frame;

    /* Main loop */
    while(1){
        int ret = poll(&pfd, 1, -1);
        if(ret > 0){
            lseek(gpio_fd, 0, SEEK_SET);
            char buf[2] = {0};
            if(read(gpio_fd, buf, 1) > 0 && buf[0] == '1'){
                log_message("GPIO rising edge detected, reading I2C...");
                if(i2c_read_frame(i2c_fd, &id, &x, &y, &status) == 0){
                    snprintf(logbuf, sizeof(logbuf),
                             "DeviceID=%u X=%.3f Y=%.3f Status=%u",
                             id, x, y, status);
                    log_message(logbuf);

                    frame.device_id = htons(id);
                    frame.cord_x = x;
                    frame.cord_y = y;
                    frame.status = htons(status);

                    if(pipe_fd >= 0){
                        ssize_t written = write(pipe_fd, &frame, sizeof(gps_frame));
                        if (written != sizeof(frame)){
                            if(errno == EPIPE || errno == ENXIO){
                                log_message("Reader disconnected from FIFO, exitig...");
                                handle_signal(SIGPIPE);
                            }
                            else{
                                log_message("Failed to write full gps_frame to pipe");
                            }
                        }
                        else{
                            log_message("Gps_frame send successfully to pipe");
                        }
                    }
                }
            }
        } else if(ret < 0){
            log_message("Poll error on GPIO");
            break;
        }
    }

    /* Clean close */
    if(i2c_fd >= 0) close(i2c_fd);
    if(gpio_fd >= 0) close(gpio_fd);
    if(pipe_fd >= 0) close(pipe_fd);

    return 0;
}