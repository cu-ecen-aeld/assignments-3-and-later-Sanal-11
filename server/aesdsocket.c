#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define PORT 9000
#define BACKLOG 10
#define FILEPATH "/var/tmp/aesdsocketdata"

int server_fd = -1;
int client_fd = -1;
FILE *file = NULL;

void clean_exit(int signal_number) {
    syslog(LOG_INFO, "Caught signal, exiting");

    if (client_fd != -1) close(client_fd);
    if (server_fd != -1) close(server_fd);
    unlink(FILEPATH);

    closelog();
    exit(0);
}

void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = clean_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char client_ip[INET_ADDRSTRLEN];
    char *recv_buf = NULL;
    // size_t recv_buf_size = 0;
    ssize_t bytes_read;

    openlog("aesdsocket", LOG_PID, LOG_USER);
    setup_signal_handler();

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        return -1;
    }

    // Allow address reuse
    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // Bind
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Bind failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    // Listen
    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listen failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    // Loop to accept connections
    while (1) {
        addr_size = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd == -1) {
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // Receive data until newline
        recv_buf = NULL;
        // recv_buf_size = 0;
        char buffer[1024];
        size_t total_len = 0;
        int newline_found = 0;

        while (!newline_found) {
            bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) break;

            char *temp = realloc(recv_buf, total_len + bytes_read + 1);
            if (!temp) {
                syslog(LOG_ERR, "Memory allocation failed");
                break;
            }
            recv_buf = temp;
            memcpy(recv_buf + total_len, buffer, bytes_read);
            total_len += bytes_read;
            recv_buf[total_len] = '\0';

            if (strchr(buffer, '\n')) newline_found = 1;
        }

        // Append to file
        file = fopen(FILEPATH, "a");
        if (file) {
            fwrite(recv_buf, 1, total_len, file);
            fclose(file);
        }

        // Send full file back
        file = fopen(FILEPATH, "r");
        if (file) {
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                send(client_fd, buffer, bytes_read, 0);
            }
            fclose(file);
        }

        free(recv_buf);
        recv_buf = NULL;

        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        close(client_fd);
        client_fd = -1;
    }

    clean_exit(0);
    return 0;
}
