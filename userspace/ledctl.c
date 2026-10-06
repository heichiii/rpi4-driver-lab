#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    char command[2];
    char state[2];
    char extra;
    size_t received = 0;
    ssize_t ret;
    int status = EXIT_FAILURE;
    int fd;

    if (argc != 2 || (strcmp(argv[1], "0") && strcmp(argv[1], "1"))) {
        fprintf(stderr, "Usage: %s 0|1\n", argv[0]);
        return EXIT_FAILURE;
    }
    command[0] = argv[1][0];
    command[1] = '\n';
    do {
        fd = open("/dev/rpi_gpio", O_RDWR | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        perror("open /dev/rpi_gpio");
        return EXIT_FAILURE;
    }
    do {
        ret = write(fd, command, sizeof(command));
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        perror("write");
        goto out;
    }
    if (ret != sizeof(command)) {
        fprintf(stderr, "Short write: %zd bytes\n", ret);
        goto out;
    }
    while (received < sizeof(state)) {
        ret = read(fd, state + received, sizeof(state) - received);
        if (ret < 0 && errno == EINTR)
            continue;
        if (ret <= 0) {
            if (ret < 0)
                perror("read");
            else
                fprintf(stderr, "Unexpected EOF\n");
            goto out;
        }
        received += (size_t)ret;
    }
    if (memcmp(state, command, sizeof(state))) {
        fprintf(stderr, "GPIO level differs from requested state\n");
        goto out;
    }
    do {
        ret = read(fd, &extra, 1);
    } while (ret < 0 && errno == EINTR);
    if (ret != 0) {
        if (ret < 0)
            perror("read EOF");
        else
            fprintf(stderr, "Expected EOF\n");
        goto out;
    }
    printf("PASS: LED %s; GPIO read-back=%c; EOF verified.\n",
           command[0] == '1' ? "ON" : "OFF", state[0]);
    status = EXIT_SUCCESS;
out:
    if (close(fd) < 0) {
        perror("close");
        status = EXIT_FAILURE;
    }
    return status;
}
