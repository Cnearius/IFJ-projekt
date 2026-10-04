CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -g

TARGET = compiler

SRC_FILES := $(shell find . -maxdepth 2 -type f -name "*.c")
HDR_FILES := $(shell find . -maxdepth 2 -type f -name "*.h")
ALL_FILES := $(SRC_FILES) $(HDR_FILES)

.PHONY: all clean format check-format cppcheck valgrind

all: $(TARGET)

$(TARGET): $(SRC_FILES)
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

clean:
	rm -f $(TARGET)