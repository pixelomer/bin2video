SOURCES := $(wildcard src/*.c)
HEADERS := $(wildcard src/*.h)

bin2video: $(SOURCES) $(HEADERS) Makefile
	$(CC) -Werror -Wall -Wextra -Wpedantic -lm -O3 $(SOURCES) -o $@
