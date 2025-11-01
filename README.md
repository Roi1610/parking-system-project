# Embedded Parking System Project

## Introduction
The **Embedded Parking System Project** is an academic demonstration of an smart parking management system.  
It integrates embedded hardware (STM32 and BeagleBone) with a Linux-based server and database in real time.


This project demonstrates key concepts in **embedded systems**, **network communication**, and using **C** and **C++**.

---

## Table of Contents
1. [Project Overview](#project-overview)
2. [System Architecture](#system-architecture)
3. [Features](#features)
4. [Project Structure](#project-structure)
5. [Dependencies](#dependencies)
6. [Installation & Setup](#installation--setup)
7. [Building and Running](#building-and-running)
8. [Configuration](#configuration)
9. [Database](#database)

---

## Project Overview

- The **STM32** 
Simulates GPS signals with parking start or end status and client ID, the information is transferred via I2C
and via an additional pin that simulates Interrupt (since the BeagleBone does not support Interrupt) 
to the BeagleBone.

- The **BeagleBone** 
The I2C daemon program receives an interrupt and then reads the information on the I2C and passes the information via a pipe to the Client daemon program.
The client program passes its information via TCP to the Linux server.

- The **Linux Server** 
Saves the information in a SQLite database, waits for the same information with a parking completion status,
and then updates the parking cost according to the parking time and a predetermined rate.
Another program allows updating prices in the database and adding or removing cities from it.

---

## System Architecture
```
[STM32 Microcontroller]
        │
        │  (I²C Communication)
        ▼
[BeagleBone Board]
   ├── I2C_DAEMON  (collects data)
        │
        │  (IPC pipe)
        ▼
   └── CLIENT_DAEMON (forwards data)
        │
        │  (TCP/IP Socket)
        ▼
[Linux Server]
   ├── SERVER (C++)
   ├── SQLite Database
   └── PRICE_UPDATER & Utilities
```

---

## Features
- Real-time calculation of parking time  
- I²C and TCP/IP communication between hardware and server  
- Local database storage using SQLite  
- Dynamic pricing logic  
- Modular design with independent build targets  
- Compatible with Linux and embedded ARM environments  

---

## Project Structure
```
parking-system-project/
├── BeagleBone/
│   ├── i2c_demon/
│   │   ├── i2c_daemon.c
│   │   ├── i2c_master.c
│   │   ├── i2c_master.h
│   │   ├── protocol.h
│   │   ├── I2C_DAEMON
│   │   └── Makefile
│   ├── tcp_client_demon/
│   │   ├── client.c
│   │   ├── client.h
│   │   ├── protocol.h
│   │   ├── CLIENT_DAEMON
│   │   └── Makefile
│   ├── service/
│       ├── start_daemons.sh
│       ├── my_daemons.service
│       ├── readme.txt
│    
│
├── server/
│   ├── main.cpp
│   ├── server.cpp / server.h
│   ├── utils.cpp / utils.h
│   ├── sqlite3.c / sqlite3.h
│   ├── price_updater.cpp
│   ├── config.h
│   ├── data.db
│   ├── SERVER
│   ├── PRICE_UPDATER
│   ├── show_db.sh
│   └── Makefile
│
└── stm32/
    └── Coordinate_generator.zip
```

---

## Dependencies

### On **Linux Server**
- GCC / G++ compiler  
- SQLite3 library  
- GNU Make  
- Bash shell  

### On **BeagleBone**
- ARM GCC cross-compiler (e.g., `arm-linux-gnueabihf-gcc`)  
- Enabled I²C drivers (`/dev/i2c-*`)  
- Network connectivity to the Linux server  

### On **STM32**
- **STM32CubeIDE** (for building and flashing firmware)  
- STM32 development board connected via USB  
- I²C interface configured  

### Wiring between **STM32** & **BeagleBone**
                 STM                       BBG
        (I2C-SDA) - PB_11 <---------> P9_20 - (I2C-SDA)
        (I2C-SCL) - PB_10 <---------> P9_19 - (I2C-SCL)
        ("POLLUP") - PE_15 <--------> P9_23 - (GPIO_49 "POLLUP")
                     GND <----------> GND

---

## Installation & Setup

### 1. Clone the Repository
```bash
git clone https://github.com/yourusername/parking-system-project.git
cd parking-system-project
```

### 2. Setup the Server Environment
Install SQLite development libraries if not already installed:
```bash
sudo apt update
sudo apt install build-essential libsqlite3-dev
```

### 3. Configure BeagleBone Network
Ensure both BeagleBone and the Linux server are on the same subnet.  
Edit `BeagleBone/tcp_client_demon/client.c` to set the correct **server IP** and **port number**.

---

## Building and Running

### On the Linux Server
#### Build:
```bash
cd server
make
```

#### Run the server:
```bash
./SERVER &
```

#### Run the price updater:
```bash
./PRICE_UPDATER &
```

#### View the current database:
```bash
./show_db.sh
```

---

### On the BeagleBone

#### Build and copy the I²C Daemon and the TCP Client Daemon on linux pc:
```bash
cd BeagleBone/i2c_demon
make
scp I2C_DAEMON debian@192.168.7.2:/home/debian/embedded

cd ../tcp_client_demon
make
scp CLIENT_DAEMON debian@192.168.7.2:/home/debian/embedded

```
#### Run the I²C Daemon and the TCP Client Daemon:
```bash
sudo ./CLIENT_DAEMON
sudo ./I2C_DAEMON

```
#### To automatically load programs after reboot:
```bash
sudo mv my_daemons.service /etc/systemd/system
sudo systemctl daemon-reload
sudo systemctl enable my_daemons.service.service

```

### On the STM32 Microcontroller
1. Unzip `stm32/Coordinate_generator.zip`.
2. Open the project in **STM32CubeIDE**.
3. Connect your STM32 board via USB.
4. Click **Build** → **Run** → **Debug As → STM32 MCU C/C++ Application**.
5. The firmware will be compiled and **flashed to the board** automatically.

Make sure the SDA/SCL lines are connected between the STM32 and the BeagleBone as explained for proper I²C communication.

---

## Configuration
Edit `server/config.h` to adjust:
- Database path  
- TCP port  
- Log settings  

Example:
```c
#define SERVER_PORT 5555
#define DB_PATH "data.db"
```

---

## Database
The SQLite database (`data.db`) stores:
- Parking slot coordinates  
- Vehicle entry/exit logs  
- Pricing and timestamps  


---



