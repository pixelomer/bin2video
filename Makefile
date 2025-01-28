SOURCES := $(wildcard src/*.c)
HEADERS := $(wildcard src/*.h)

bin2video: $(SOURCES) $(HEADERS) Makefile
	$(CC) $(CFLAGS) -o $@ -Werror -Wall -Wextra -Wpedantic -O3 $(SOURCES) -lm
