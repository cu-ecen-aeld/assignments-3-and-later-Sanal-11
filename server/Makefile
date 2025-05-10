CC ?= gcc
CFLAGS := -Wall -Werror -g
TARGET := aesdsocket
CLIENT_TARGET := test_client

all: $(TARGET) $(CLIENT_TARGET)
default: all

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c
	
# Build client
$(CLIENT_TARGET):  simple_client.c
	$(CC) $(CFLAGS) -o $(CLIENT_TARGET) simple_client.c
	

clean:
	rm -f $(TARGET)
