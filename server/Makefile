CC ?= gcc
CFLAGS ?= 
CFLAGS += -Wall -Werror -g
TARGET := aesdsocket

$(info CC=$(CC))
$(info CFLAGS=$(CFLAGS))
$(info LDFLAGS=$(LDFLAGS))

ifdef CROSS_COMPILE
    CC := $(CROSS_COMPILE)gcc
endif

all: $(TARGET) $(CLIENT_TARGET)
default: all

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)
