CC = arm-linux-gnueabihf-gcc
CFLAGS = -Wall -ggdb

SRCS = i2c_master.c i2c_daemon.c
HDRS = i2c_master.h protocol.h

TARGET = I2C_DAEMON

all:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

.PHONY:clean

clean:
	rm -f $(TARGET)