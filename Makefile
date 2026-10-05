CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

TARGET = ifj26

SRC_DIR = src
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
HDRS = $(wildcard $(SRC_DIR)/*.h)

.PHONY: all build clean

all: build

build: $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@

clean:

	rm -rf $(BUILD_DIR) $(TARGET)

run:
	./ifj26