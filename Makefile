CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -g

TARGET = compiler

SRC_FILES := $(shell find . -maxdepth 2 -type f -name "*.c")
HDR_FILES := $(shell find . -maxdepth 2 -type f -name "*.h")
ALL_FILES := $(SRC_FILES) $(HDR_FILES)

.PHONY: all clean format check-format cppcheck valgrind doc test

all: $(TARGET)

$(TARGET): $(SRC_FILES) $(HDR_FILES)
	$(CC) $(CFLAGS) $(SRC_FILES) -o $(TARGET)

format:
	@if command -v clang-format >/dev/null 2>&1; then \
		clang-format -i --style=file $(ALL_FILES); \
		echo "Kod bol naformatovany"; \
	else \
		echo "Varovanie: clang-format nie je nainstalovany v systeme"; \
	fi

check-format:
	@if command -v clang-format >/dev/null 2>&1; then \
		clang-format --dry-run --Werror --style=file $(ALL_FILES); \
		echo "Formatovanie prebehlo uspesne."; \
	else \
		echo "clang-format nie je nainstalovany v systeme"; \
	fi

cppcheck:
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --suppress=missingIncludeSystem --error-exitcode=1 $(ALL_FILES); \
	else \
		echo "Varovanie: cppcheck nie je nainstalovany v systeme"; \
	fi

valgrind: $(TARGET)
	@if command -v valgrind >/dev/null 2>&1; then \
		valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./"$(TARGET)"; \
	else \
		echo "Varovanie: valgrind nie je nainstalovany v systeme"; \
	fi

doc:
	@if command -v doxygen >/dev/null 2>&1; then \
		doxygen Doxyfile; \
		echo "Dokumentacia bola vygenerovana v zlozke doc/"; \
	else \
		echo "Varovanie: doxygen nie je nainstalovany v systeme"; \
	fi

clean:
	rm -rf $(TARGET) doc lexer.o $(TEST_BIN)

# ===== Unit testy lexera (GoogleTest) =====
# Ciel test vygenerovany AI (Claude).
# Prompt: "urobit make file pre testy"
# Nie su sucastou hlavneho prekladu, spustaju sa len cez "make test".
TEST_BIN = lexer_test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): tests/lexer/lexer_test.cpp lexer/lexer.c lexer/lexer.h lexer/token.h
	$(CC) $(CFLAGS) -c lexer/lexer.c -o lexer.o
	g++ -std=c++17 -Wall -Wextra -g tests/lexer/lexer_test.cpp lexer.o -lgtest -lgtest_main -pthread -o $(TEST_BIN)