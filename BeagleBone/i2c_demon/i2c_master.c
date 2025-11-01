#include "i2c_master.h"

void log_message(const char *message){
    FILE *log=fopen(LOG_FILE,"a");
    if(!log){
        perror("log_message fopen");
        return;
    }
    time_t now=time(NULL);
    char ts[64];
    strftime(ts,sizeof(ts),"%d-%m-%Y %H:%M:%S",localtime(&now));
    fprintf(log,"[%s] %s\n\n",ts,message);
    fclose(log);
}

int gpio_init(int gpio_num){
    char path[128];
    int fd;

    snprintf(path,sizeof(path),"%s/gpio%d",GPIO_BASE_PATH,gpio_num);
    if(access(path,F_OK)!=0){
        fd=open(GPIO_BASE_PATH "/export",O_WRONLY);
        if(fd>=0){
            dprintf(fd,"%d",gpio_num);
            close(fd);
            usleep(100000);
            log_message("GPIO exported");
        } else {
            log_message("Failed to export GPIO");
            return -1;
        }
    }

    snprintf(path,sizeof(path),"%s/gpio%d/direction",GPIO_BASE_PATH,gpio_num);
    fd=open(path,O_WRONLY);
    if(fd>=0){ write(fd,"in",2); close(fd); log_message("GPIO direction set to input"); }
    else { log_message("Failed to set GPIO direction"); return -1; }

    snprintf(path,sizeof(path),"%s/gpio%d/edge",GPIO_BASE_PATH,gpio_num);
    fd=open(path,O_WRONLY);
    if(fd>=0){ write(fd,"rising",6); close(fd); log_message("GPIO edge set to rising"); }
    else { log_message("Failed to set GPIO edge"); return -1; }

    snprintf(path,sizeof(path),"%s/gpio%d/value",GPIO_BASE_PATH,gpio_num);
    fd=open(path,O_RDONLY | O_NONBLOCK);
    if(fd<0){ log_message("Failed to open GPIO value file"); return -1; }

    char buf[8]; lseek(fd,0,SEEK_SET); read(fd,buf,sizeof(buf));
    return fd;
}


int i2c_read_frame(int fd,uint16_t *device_id,float *cord_x,float *cord_y,uint16_t *status){
    uint8_t raw[12];
    ssize_t n=read(fd,raw,sizeof(raw));
    if(n!=sizeof(raw)){
        char err[64]; snprintf(err,sizeof(err),"I2C read failed (got %zd bytes)",n);
        log_message(err);
        return -1;
    }
    *status    = swap16(*(uint16_t*)&raw[0]);
    *cord_y    = swap_float(&raw[2]);
    *cord_x    = swap_float(&raw[6]);
    *device_id = swap16(*(uint16_t*)&raw[10]);
    return 0;
}

extern int pipe_fd;
extern int i2c_fd;
extern int gpio_fd;

void handle_signal(int signal){
    switch (signal)
    {
    case SIGINT:
        log_message("Caught SIGINT, Closing");
        close(i2c_fd);
        close(gpio_fd);
        if(pipe_fd>=0) close(pipe_fd);
        exit(EXIT_FAILURE);
        break;

    case SIGTERM:
        log_message("Caught SIGTERM, Closing");
        close(i2c_fd);
        close(gpio_fd);
        if(pipe_fd>=0) close(pipe_fd);
        exit(EXIT_FAILURE);
        break;

    case SIGHUP:
        log_message("Caught SIGHUP, Closing");
        close(i2c_fd);
        close(gpio_fd);
        if(pipe_fd>=0) close(pipe_fd);
        exit(EXIT_FAILURE);
        break;

    case SIGQUIT:
        log_message("Caught SIGQUIT, Closing");
        close(i2c_fd);
        close(gpio_fd);
        if(pipe_fd>=0) close(pipe_fd);
        exit(EXIT_FAILURE);
        break;

     case SIGPIPE:
        log_message("Caught SIGPIPE, Closing");
        close(i2c_fd);
        close(gpio_fd);
        if(pipe_fd>=0) close(pipe_fd);
        exit(EXIT_FAILURE);
        break;

    default:
        log_message("Caught signal, Closing");
        close(i2c_fd);
        close(gpio_fd);
        if(pipe_fd>=0) close(pipe_fd);
        exit(EXIT_FAILURE);
        break;
    }
}

