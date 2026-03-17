CC       := gcc
CFLAGS   := -Wall -Wextra -pthread -Iinclude
LDFLAGS  := -pthread

SRC_DIR  := src
BIN_DIR  := bin
INCL_DIR := include

MANAGER  := nfs_manager
CONSOLE  := nfs_console
CLIENT   := nfs_client

MANAGER_SRC := $(SRC_DIR)/nfs_manager.c $(SRC_DIR)/manager_core.c $(SRC_DIR)/worker_jobs.c $(SRC_DIR)/utils.c
CONSOLE_SRC := $(SRC_DIR)/nfs_console.c $(SRC_DIR)/utils.c
CLIENT_SRC  := $(SRC_DIR)/nfs_client.c $(SRC_DIR)/utils.c $(SRC_DIR)/command_exec.c

all: $(BIN_DIR)/$(MANAGER) $(BIN_DIR)/$(CONSOLE) $(BIN_DIR)/$(CLIENT)

$(BIN_DIR)/$(MANAGER): $(MANAGER_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/$(CONSOLE): $(CONSOLE_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/$(CLIENT): $(CLIENT_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)/*.o $(BIN_DIR)/$(MANAGER) $(BIN_DIR)/$(CONSOLE) $(BIN_DIR)/$(CLIENT)

.PHONY: all clean
