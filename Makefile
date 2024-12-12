CC = gcc
CFLAGS = -pedantic -Wall -Wextra -pthread -std=gnu23 -O2
LDFLAGS = -pthread
TARGET = hw4
SRCS = main.c tpool.c
BUILD_DIR = build
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS = $(SRCS:%.c=$(BUILD_DIR)/%.d)

$(TARGET): $(BUILD_DIR) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

-include $(DEPS)

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

.PHONY: clean all
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
all: clean $(TARGET)
