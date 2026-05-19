CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -O2 -Iinclude
LDFLAGS =

SRC        = src/ld.c
MAIN_SRC   = src/main.c
TEST_SRC   = tests/test_ld.c
TESTR_SRC  = tests/test_random.c
TESTB_SRC  = tests/test_brute.c
GEN_SRC    = src/generator.c
BENCH_SRC  = src/bench.c
BATCH_SRC  = src/bench_batch.c

BIN_DIR    = bin
APP        = $(BIN_DIR)/loadbalance
TEST_BIN   = $(BIN_DIR)/test_ld
TESTR_BIN  = $(BIN_DIR)/test_random
TESTB_BIN  = $(BIN_DIR)/test_brute
GEN_BIN    = $(BIN_DIR)/gen
BENCH_BIN  = $(BIN_DIR)/bench
BATCH_BIN  = $(BIN_DIR)/bench_batch

.PHONY: all test test-all clean run bench bench-batch

all: $(APP) $(GEN_BIN) $(BENCH_BIN) $(BATCH_BIN)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(APP): $(SRC) $(MAIN_SRC) include/ld.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC) $(MAIN_SRC) -o $@ $(LDFLAGS)

$(TEST_BIN): $(SRC) $(TEST_SRC) include/ld.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC) $(TEST_SRC) -o $@ $(LDFLAGS)

$(TESTR_BIN): $(SRC) $(TESTR_SRC) include/ld.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC) $(TESTR_SRC) -o $@ $(LDFLAGS)

$(GEN_BIN): $(GEN_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(GEN_SRC) -o $@ $(LDFLAGS)

$(BENCH_BIN): $(SRC) $(BENCH_SRC) include/ld.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC) $(BENCH_SRC) -o $@ $(LDFLAGS)

$(BATCH_BIN): $(SRC) $(BATCH_SRC) include/ld.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC) $(BATCH_SRC) -o $@ $(LDFLAGS)

$(TESTB_BIN): $(SRC) $(TESTB_SRC) include/ld.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC) $(TESTB_SRC) -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

test-all: $(TEST_BIN) $(TESTR_BIN) $(TESTB_BIN)
	./$(TEST_BIN)
	./$(TESTR_BIN)
	./$(TESTB_BIN)

run: $(APP)
	./$(APP) data/sample.txt all

# демо: сгенерировать инстанс и прогнать через bench
bench: $(GEN_BIN) $(BENCH_BIN)
	./$(GEN_BIN) 5 50 1 100 0.6 42 > $(BIN_DIR)/instance.txt
	./$(BENCH_BIN) $(BIN_DIR)/instance.txt

# пакетный бенч на 30 случайных инстансах со статистикой
bench-batch: $(BATCH_BIN)
	./$(BATCH_BIN) 5 50 1 100 0.6 30

clean:
	rm -rf $(BIN_DIR)
