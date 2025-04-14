#include <stdio.h>
#include <syslog.h>

int main(int argc, char *argv[]) {

    openlog("writer-util", LOG_PID | LOG_CONS, LOG_USER);

    if (argc != 3) {
        printf("Usage: %s <filename> <text>\n", argv[0]);
        syslog(LOG_USER, "Usage: %s <filename> <text>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    const char *text = argv[2];

    FILE *file = fopen(filename, "w"); // "w" = write (overwrite if exists)
    if (file == NULL) {
        perror("Error opening file");
        syslog(LOG_INFO, "Error opening file");
        return 1;
    }

    fprintf(file, "%s\n", text);
    syslog(LOG_INFO, "%s\n", text);
    fclose(file);

    printf("Wrote to file '%s': %s\n", filename, text);
    syslog(LOG_INFO, "Wrote to file '%s': %s\n", filename, text);
    return 0;
}