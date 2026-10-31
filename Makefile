# FDS-only build of libezhle. The PS2 attach symbol is stubbed: this repository
# owns FDS, not PS2.
CC      ?= cc
CXX     ?= c++
AR      ?= ar
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2 -g -ffile-prefix-map=$(CURDIR)=.
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -pedantic -O2 -g -ffile-prefix-map=$(CURDIR)=.
CPPFLAGS += -Iinclude -Isrc

SRC := \
	src/common/ezhle.c \
	src/common/ezhle_ps2_stub.c \
	src/systems/fds/fds_hle.c \
	src/systems/fds/fds_image.c

HEADERS := $(sort $(shell find include src -type f -name '*.h'))
OBJ := $(SRC:%.c=build/%.o)
LIB := build/libezhle.a
FDS_TEST := build/test_fds
IMAGE_TEST := build/test_fds_image
CPU_TEST := build/test_fds_cpu_004
CPP_TEST := build/test_cpp_smoke
FUZZ_BIN := build/fuzz_fds_image
CORPUS_DIR := corpus/fuzz_fds_image

.PHONY: all test test-cxx asan fuzz fuzz-libfuzzer fuzz-corpus clean

all: $(LIB)

$(LIB): $(OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

build/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(FDS_TEST): tests/test_fds.c $(SRC) $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(filter %.c,$^)

$(IMAGE_TEST): tests/test_fds_image.c src/systems/fds/fds_image.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(filter %.c,$^)

$(CPU_TEST): tests/fds_cpu_004_test.c $(SRC) $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(filter %.c,$^)

test: $(FDS_TEST) $(IMAGE_TEST) $(CPU_TEST)
	./$(FDS_TEST)
	./$(IMAGE_TEST)
	./$(CPU_TEST)

$(CPP_TEST): tests/test_cpp_smoke.cpp $(LIB) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $< $(LIB)

test-cxx: $(CPP_TEST)
	./$(CPP_TEST)

asan:
	$(MAKE) clean
	$(MAKE) test CFLAGS="-std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"

$(FUZZ_BIN): tests/fuzz_fds_image.c src/systems/fds/fds_image.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) -O1 -g -o $@ tests/fuzz_fds_image.c src/systems/fds/fds_image.c

fuzz-libfuzzer: tests/fuzz_fds_image.c src/systems/fds/fds_image.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) -O1 -g -fsanitize=fuzzer,address,undefined -DUSE_LIBFUZZER -fno-omit-frame-pointer -o build/fuzz_fds_image_libfuzzer tests/fuzz_fds_image.c src/systems/fds/fds_image.c
	./build/fuzz_fds_image_libfuzzer -max_total_time=60 corpus/fuzz_fds_image

fuzz: $(FUZZ_BIN) fuzz-corpus
	@for f in $(CORPUS_DIR)/*; do ./$(FUZZ_BIN) "$$f" || exit 1; done
	@./$(FUZZ_BIN) --random
	@echo "fuzz: synthetic corpus + 20000 random inputs passed (libFuzzer runtime is optional)"

fuzz-corpus:
	@mkdir -p $(CORPUS_DIR)
	@python3 tests/gen_fuzz_corpus.py $(CORPUS_DIR)

clean:
	rm -rf build $(CORPUS_DIR) corpus
