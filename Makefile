CC = gcc
CFLAGS = -Wall -fPIC
LDFLAGS = -shared

SRC_DIR = .
BUILD_DIR = .
LIB_DIR = lib

LINEAR_SRC = $(wildcard $(SRC_DIR)/linear/*.c)
POOL_SRC = $(wildcard $(SRC_DIR)/pool/*.c)
TEST_LINEAR_SRC = $(SRC_DIR)/test_linear.c
TEST_POOL_SRC = $(SRC_DIR)/test_pool.c

LINEAR_LIB = $(LIB_DIR)/liblinear.so
POOL_LIB = $(LIB_DIR)/libpool.so
TEST_LINEAR = $(BUILD_DIR)/test_linear
TEST_POOL = $(BUILD_DIR)/test_pool

$(shell mkdir -p $(LIB_DIR))

all: $(TEST_LINEAR) $(TEST_POOL)

$(LINEAR_LIB): $(LINEAR_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(POOL_LIB): $(POOL_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(TEST_LINEAR): $(TEST_LINEAR_SRC) $(LINEAR_LIB)
	$(CC) $(CFLAGS) -o $@ $^ -L$(LIB_DIR) -llinear -I$(SRC_DIR)/linear

$(TEST_POOL): $(TEST_POOL_SRC) $(POOL_LIB)
	$(CC) $(CFLAGS) -o $@ $^ -L$(LIB_DIR) -lpool -I$(SRC_DIR)/pool

clean:
	rm -rf $(LIB_DIR)
	rm -f $(TEST_LINEAR) $(TEST_POOL)

.PHONY: all clean