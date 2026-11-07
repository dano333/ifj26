CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -MMD -MP

TARGET = ifj26

SRC_DIR = src
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
HDRS = $(wildcard $(SRC_DIR)/*.h)
DEPS = $(OBJS:.o=.d)

.PHONY: all clean run check format build-check

all: $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

run: all
	./$(TARGET)

check:
	clang-format --dry-run -Werror $(SRCS) $(HDRS)

format:
	clang-format -i $(SRCS) $(HDRS)

build-check: check all