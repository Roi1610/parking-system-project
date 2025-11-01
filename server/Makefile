# Makefile for building the server and price updater (Linux)

CXX = g++
CC  = gcc

CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
CFLAGS   = -O2 -Wall -Wextra

# Source files
SRCS_CPP_SERVER   = server.cpp main.cpp utils.cpp
SRCS_CPP_UPDATER  = price_updater.cpp utils.cpp
SRCS_C            = sqlite3.c

# Objects
OBJS_SERVER   = $(SRCS_CPP_SERVER:.cpp=.o) $(SRCS_C:.c=.o)
OBJS_UPDATER  = $(SRCS_CPP_UPDATER:.cpp=.o) $(SRCS_C:.c=.o)

# Targets
TARGET_SERVER  = server
TARGET_UPDATER = price_updater

# Default target
all: $(TARGET_SERVER) $(TARGET_UPDATER)

# Compile C++ sources
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile C sources
%.o: %.c
	$(CC) $(CFLAGS) -DSQLITE_THREADSAFE=1 -fPIC -c $< -o $@

# Link server
$(TARGET_SERVER): $(OBJS_SERVER)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS_SERVER) -ldl -lpthread -lm -lrt

# Link price updater
$(TARGET_UPDATER): $(OBJS_UPDATER)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS_UPDATER) -ldl -lpthread -lm -lrt

# Clean build artifacts
clean:
	rm -f *.o $(TARGET_SERVER) $(TARGET_UPDATER) data.db server.log prices.txt

.PHONY: all clean