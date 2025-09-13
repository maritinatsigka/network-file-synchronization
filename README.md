# Network File Synchronization System (NFS)

This repository contains my implementation of the **System Programming (2025)** second course project at the National and Kapodistrian University of Athens (NKUA).  
The project is written in **C** and demonstrates advanced system programming concepts, including **sockets, multithreading, condition variables, bounded buffers, and low-level I/O system calls**.

---

## 📌 Project Overview

The goal of this project was to extend the first assignment (local File Synchronization Service) into a **network-based synchronization system**.  
The system automatically synchronizes files between **remote directories** in real time, using **TCP sockets** for communication and **worker threads** for concurrent file transfers.

The system is composed of the following components:

- **nfs_manager**  
  Central process that manages synchronization tasks.  
  - Loads directory pairs from a configuration file.  
  - Connects to remote `nfs_client` instances via sockets.  
  - Uses a **thread pool** with a bounded buffer to schedule sync tasks.  
  - Handles console commands: `add`, `cancel`, `shutdown`.  

- **nfs_client**  
  Lightweight server running on each host.  
  - Listens on a port for manager requests.  
  - Supported operations:  
    - `LIST <dir>` → returns list of files in a directory  
    - `PULL <file>` → sends file contents to the requester  
    - `PUSH <file>` → receives and writes file contents  

- **nfs_console**  
  Command-line interface for user interaction.  
  - Sends commands to the manager over TCP.  
  - Logs user commands and displays responses.  
  - Commands: `add`, `cancel`, `shutdown`.  

---

## ⚙️ Features
- Remote synchronization across hosts using **TCP sockets**.  
- **Thread pool** with worker threads for concurrent tasks.  
- **Bounded buffer** + **condition variables** for scheduling.  
- Full support for `add`, `cancel`, `shutdown` commands.  
- Structured logging for both manager and console.  
- File transfers implemented with **low-level syscalls** (`open`, `read`, `write`, `close`).  
- Robust error handling with `strerror(errno)`.  

---

## ▶️ Run Instructions
1. **Start the Manager**
   ```bash
   ./bin/nfs_manager -l manager_log.txt -c config.txt -n 5 -p 8080 -b 10
   ```
   - -l → manager log file
   - -c → configuration file (<source@host:port> <target@host:port>)
   - -n → max worker threads
   - -p → port for console connections
   - -b → bounded buffer size
2. **Start a Client**
   ```bash
   ./bin/nfs_client -p 8080
   ```
   - -p → port where client listens
3. **Start the Console**
   ```bash
   ./bin/nfs_console -l console_log.txt -h 127.0.0.1 -p 8080
   ```
   - -l → console log file
   - -h → manager host IP
   - -p → manager port
4. **Console Commands**
   - add <source> <target> → add new directory pair for synchronization
   - cancel <source> → cancel synchronization for a directory
   - shutdown → gracefully stop manager and workers

---

## 📄 Notes
- **Configuration file (config.txt)**
   ```bash
   /dir1@127.0.0.1:8080 /dir2@127.0.0.1:8090
   /src@192.168.1.5:8000 /backup@192.168.1.6:9000
   ```
- **Manager log format**
  ```bash
   [TIMESTAMP] [SOURCE] [TARGET] [THREAD_ID] [OPERATION] [RESULT] [DETAILS]
   ```

  ---

- **Assumptions**
  - Flat directories only (no subdirectories).  
  - One-to-one mapping of source → target.  
  - Files are always overwritten without timestamp checking.  
  - Non-blocking sockets are used to avoid deadlocks.  
  - Errors are logged with `strerror(errno)`.  
