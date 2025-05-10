CC ?= gcc
CFLAGS := -Wall -Werror -g
TARGET := aesdsocket

all: $(TARGET) $(CLIENT_TARGET)
default: all

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -f $(TARGET)
