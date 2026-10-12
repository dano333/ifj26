/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: makefile for the project
*/

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -MMD -MP

TARGET = ifj26

SRC_DIR = src
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
HDRS = $(wildcard $(SRC_DIR)/*.h)
DEPS = $(OBJS:.o=.d)

-include $(DEPS)

.PHONY: all build clean run check format build-check

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

check:
	clang-format --dry-run -Werror $(SRCS) $(HDRS)

format:
	clang-format -i $(SRCS) $(HDRS)

build-check: check build
