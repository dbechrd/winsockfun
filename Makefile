BINARY = sock_serv
OBJECTS = main.o
CC = gcc
CFLAGS = -std=c99 -g -MMD -O0 -Wall -Wextra -Werror -Wno-unused-function

$(BINARY): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(BINARY) $(OBJECTS)

.PHONY: clean
