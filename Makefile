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
CPP_TEST := build/test_cpp_smoke

.PHONY: all test test-cxx asan clean

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

test: $(FDS_TEST) $(IMAGE_TEST)
	./$(FDS_TEST)
	./$(IMAGE_TEST)

$(CPP_TEST): tests/test_cpp_smoke.cpp $(LIB) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $< $(LIB)

test-cxx: $(CPP_TEST)
	./$(CPP_TEST)

asan:
	$(MAKE) clean
	$(MAKE) test CFLAGS="-std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"

clean:
	rm -rf build
