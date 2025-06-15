#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/types.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdbool.h>

#define PORT 9000
#define BACKLOG 10
//#define FILE_PATH "/var/tmp/aesdsocketdata"
#define FILE_PATH "/dev/aesdchar"
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

int server_fd = -1;
// FILE *file = NULL;

void daemonize()
{
    pid_t pid;

    syslog(LOG_INFO, "Daemonizing process...");
    // Fork the first time
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // parent exits

    // Create new session and process group
    if (setsid() < 0) exit(EXIT_FAILURE);

    // Ignore SIGHUP and fork again
    signal(SIGHUP, SIG_IGN);
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    // Set new file permissions
    umask(0);

    // Change to root directory
    if (chdir("/") < 0) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    // Close all open file descriptors
    long max_fd = sysconf(_SC_OPEN_MAX);
    for (int fd = 0; fd < max_fd; fd++) {
        if (fd != server_fd && fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
            close(fd);
        }
    }

    // Redirect stdin, stdout, stderr to /dev/null
    int fd0 = open("/dev/null", O_RDWR);
    if (fd0 != -1) {
       dup2(fd0, STDIN_FILENO);  // 0
       dup2(fd0, STDOUT_FILENO); // 1
       dup2(fd0, STDERR_FILENO); // 2
        if (fd0 > STDERR_FILENO) close(fd0); // avoid FD leak
    }

}


void clean_exit(int signal_number) {
    syslog(LOG_INFO, "Caught signal, exiting");
    exit_requested = true;
}

void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = clean_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
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

void *timestamp_thread_func(void *arg) {
    (void)arg; // unused
    while (!exit_requested) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);

        char time_str[100];
        strftime(time_str, sizeof(time_str), "%a, %d %b %Y %H:%M:%S %z", tm_info);

        char timestamp_line[150];
        snprintf(timestamp_line, sizeof(timestamp_line), "timestamp:%s\n", time_str);

        pthread_mutex_lock(&file_mutex);
        FILE *fp = fopen(FILE_PATH, "a");
        if (fp) {
            // fputs(timestamp_line, fp);
            fclose(fp);
        } else {
            syslog(LOG_ERR, "Failed to open file for timestamp: %s", strerror(errno));
        }
        pthread_mutex_unlock(&file_mutex);

        for (int i = 0; i < 10 && !exit_requested; i++) sleep(1);
    }
    return NULL;
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
        char *temp = realloc(accumulated_data, total_size + bytes_read + 1);
        if (!temp) {
            syslog(LOG_ERR, "Memory allocation failed");
            free(accumulated_data);
            return NULL;
        }
        accumulated_data = temp;
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
    //close(client_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char client_ip[INET_ADDRSTRLEN];
 //   char *recv_buf = NULL;
    // size_t recv_buf_size = 0;
 //   ssize_t bytes_read;
    int is_daemon = 0;

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        // daemonize();
        is_daemon = 1;
    }

    openlog("aesdsocket", LOG_PID, LOG_DAEMON);
    setup_signal_handler();


    // socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        return -1;
    }

    // address reuse
    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // binding
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Bind failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    // wait for client to connect 
    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listen failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    if (is_daemon) {
        syslog(LOG_INFO, "Daemonizing process...");
        daemonize();
        closelog();
        openlog("aesdsocket", LOG_PID, LOG_DAEMON);
    }

    pthread_t timestamp_thread;
    if (pthread_create(&timestamp_thread, NULL, timestamp_thread_func, NULL) != 0) {
        syslog(LOG_ERR, "Failed to create timestamp thread");
        exit(EXIT_FAILURE);
    }


    // Loop to accept connections
    while (!exit_requested) {
        addr_size = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd == -1) {
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
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
    pthread_join(timestamp_thread, NULL);
    cleanup_thread_list();
    close(server_fd);
    remove(FILE_PATH);
    closelog();
    return 0;
}
