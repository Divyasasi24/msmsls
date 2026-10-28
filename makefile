# ---------------------------------------------------------
#  Simple Makefile - Medical Supply Management System
#  Folders: src/*.c   include/medsupply.h   tests/*.cpp
#  Commands: make | make test | make shell-test | make valgrind
#            make coverage | make check | make clean
# ---------------------------------------------------------

CC       = gcc
CXX      = g++
CFLAGS   = -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude -Wall -Wextra -g -O0
CXXFLAGS = -std=c++17 -Iinclude -Wall -Wextra -g -O0
EXTRA    =                  # "make coverage" fills this with --coverage

SRC      = $(wildcard src/*.c)                    # all C files
LIB_SRC  = $(filter-out src/main.c,$(SRC))        # all C files except main.c
TESTS    = $(wildcard tests/*.cpp)                # GoogleTest files
OBJ      = $(patsubst src/%.c,build/%.o,$(LIB_SRC))

.PHONY: all test shell-test valgrind coverage check clean

# 1) Build the program:  make
all: medsupply

medsupply: $(OBJ) build/main.o
	$(CC) $(EXTRA) -o $@ $^

# Turn each src/x.c into build/x.o
build/%.o: src/%.c include/medsupply.h
	mkdir -p build
	$(CC) $(CFLAGS) $(EXTRA) -c $< -o $@

# 2) Unit tests (GoogleTest):  make test
build/tests: $(TESTS) $(OBJ)
	$(CXX) $(CXXFLAGS) $(EXTRA) -o $@ $^ -lgtest_main -lgtest -pthread

test: build/tests
	./build/tests

# 3) Shell tests (run_tests.sh):  make shell-test
shell-test: medsupply
	BIN=$(abspath medsupply) bash run_tests.sh

# 4) Memory check:  make valgrind
valgrind: build/tests
	valgrind --leak-check=full --error-exitcode=1 ./build/tests

# 5) Code coverage:  make coverage
coverage: clean
	$(MAKE) EXTRA=--coverage test shell-test
	gcov -b -c -o build $(LIB_SRC) | grep -A1 "^File 'src/"

# 6) Static analysis:  make check
check:
	cppcheck --enable=warning,performance --suppress=missingIncludeSystem \
	         --std=c11 --language=c --error-exitcode=1 -Iinclude src

# 7) Delete everything built:  make clean
clean:
	rm -rf build medsupply *.gcov *.gcda *.gcno
