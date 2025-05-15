CC ?= gcc
CFLAGS := -Wall -Werror -g
TARGET := aesdsocket

ifdef CROSS_COMPILE
    CC := $(CROSS_COMPILE)gcc
endif

all: $(TARGET) $(CLIENT_TARGET)
default: all

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -f $(TARGET)
