CC = arm-linux-gnueabihf-gcc
CFLAGS = -Wall -ggdb

SRCS = client.c
HDRS = client.h protocol.h

TARGET = CLIENT_DAEMON

all:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

.PHONY:clean

clean:
	rm -f $(TARGET)