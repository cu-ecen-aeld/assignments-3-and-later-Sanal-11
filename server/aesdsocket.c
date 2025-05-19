#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>

#define PORT 9000
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

// sig exit flag
static volatile bool exit_requested = false;

//mutex for file access
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

//thread node, singly linked list
typedef struct thread_node {
    pthread_t thread_id;
    int client_fd;
    struct thread_node *next;
} thread_node_t;

thread_node_t *head = NULL;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

void signal_handler(int signum) {
    exit_requested = true;
    syslog(LOG_INFO, "Caught signal, exiting");
}

//cleanup helper
void add_thread_to_list(pthread_t thread_id, int client_fd) {
    thread_node_t *node = malloc(sizeof(thread_node_t));
    node->thread_id = thread_id;
    node->client_fd = client_fd;
    node->next = NULL;

    pthread_mutex_lock(&list_mutex);
    node->next = head;
    head = node;
    pthread_mutex_unlock(&list_mutex);
}

//remove thread from list
void cleanup_thread_list() {
    pthread_mutex_lock(&list_mutex);
    thread_node_t *current = head;
    while (current) {
        pthread_join(current->thread_id, NULL);
        close(current->client_fd);
        thread_node_t *tmp = current;
        current = current->next;
        free(tmp);
    }
    head = NULL;
    pthread_mutex_unlock(&list_mutex);
}

//thread function for client
void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    FILE *fp;

    //fetch until newline
    char *accumulated_data = NULL;
    size_t total_size = 0;

    while ((bytes_read = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
        accumulated_data = realloc(accumulated_data, total_size + bytes_read + 1);
        memcpy(accumulated_data + total_size, buffer, bytes_read);
        total_size += bytes_read;
        accumulated_data[total_size] = '\0';

        if (strchr(buffer, '\n')) break;
    }

    if (bytes_read > 0 && accumulated_data) {
        pthread_mutex_lock(&file_mutex);
        fp = fopen(FILE_PATH, "a");
        fwrite(accumulated_data, 1, total_size, fp);
        fclose(fp);
        pthread_mutex_unlock(&file_mutex);

        pthread_mutex_lock(&file_mutex);
        fp = fopen(FILE_PATH, "r");
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
            send(client_fd, buffer, bytes_read, 0);
        }
        fclose(fp);
        pthread_mutex_unlock(&file_mutex);
    }

    free(accumulated_data);
    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    int sockfd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    //syslog
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    //create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "Bind failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 10);

    //main loop
    while (!exit_requested) {
        client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0) {
            if (exit_requested) break;
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        pthread_t tid;

        if (pthread_create(&tid, NULL, handle_client, client_fd_ptr) != 0) {
            syslog(LOG_ERR, "Failed to create thread");
            close(client_fd);
            free(client_fd_ptr);
            continue;
        }

        add_thread_to_list(tid, client_fd);
    }

    //cleanup
    close(sockfd);
    cleanup_thread_list();
    remove(FILE_PATH);
    closelog();
    return 0;
}
