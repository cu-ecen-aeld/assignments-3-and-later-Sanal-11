#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP   "127.0.0.1"  // Change to server's IP if needed
#define SERVER_PORT 9000

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char send_buf[] = "Hello, AESD socket!\n";
    char recv_buf[1024];
    ssize_t bytes_recv;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        close(sock);
        return 1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    // Send message (must end in newline to trigger server reply)
    send(sock, send_buf, strlen(send_buf), 0);

    // Read response from server (entire file contents)
    printf("Server response:\n");
    while ((bytes_recv = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0)) > 0) {
        recv_buf[bytes_recv] = '\0';
        printf("%s", recv_buf);
    }

    close(sock);
    return 0;
}
