#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>

int main(int argc, char *argv[]) {

    openlog("writer-util", LOG_PID | LOG_CONS, LOG_USER);

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <filename> <text>\n", argv[0]);
        syslog(LOG_ERR, "Invalid number of arguments. Usage: %s <filename> <text>", argv[0]);
        closelog();
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    const char *text = argv[2];

    fprintf(stdout, "finename: %s  content: %s\n", filename, text);

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening file");
        syslog(LOG_ERR, "Error opening file '%s': %m", filename); // %m includes strerror(errno)
        closelog();
        return EXIT_FAILURE;
    }

    fprintf(file, "%s\n", text);
    fclose(file);

    printf("Wrote to file '%s': %s\n", filename, text);
    syslog(LOG_INFO, "Wrote to file '%s': %s", filename, text);

    closelog();
    return EXIT_SUCCESS;
}
