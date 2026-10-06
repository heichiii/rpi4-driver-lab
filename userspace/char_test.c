#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/rpi_gpio"
#define BUFFER_SIZE 128

/* This test targets the driver's replace-on-write buffer interface. */
int main(int argc, char *argv[])
{
    const char *message = argc > 1 ? argv[1] : "hello rpi_gpio";
    const char *path = argc > 2 ? argv[2] : DEVICE_PATH;
    unsigned char buffer[BUFFER_SIZE];
    unsigned char extra;
    size_t length = strlen(message);
    size_t received = 0;
    ssize_t result;
    int status = EXIT_FAILURE;
    int fd;

    if (argc > 3) {
        fprintf(stderr, "Usage: %s [message [device]]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (length == 0 || length > BUFFER_SIZE) {
        fprintf(stderr, "Message must contain 1 to %d bytes.\n", BUFFER_SIZE);
        return EXIT_FAILURE;
    }

    do {
        fd = open(path, O_RDWR | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        perror(path);
        return EXIT_FAILURE;
    }

    do {
        result = write(fd, message, length);
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        perror("write");
        goto out;
    }
    if ((size_t)result != length) {
        /* Retrying a partial write would replace the earlier data. */
        fprintf(stderr, "Short write: expected %zu bytes, got %zd.\n",
                length, result);
        goto out;
    }
    printf("Wrote %zd bytes.\n", result);

    /* The driver resets this file's position on a successful write. */
    while (received < length) {
        size_t count = length - received;

        /* Small reads exercise the driver's file-position handling. */
        if (count > 3)
            count = 3;
        do {
            result = read(fd, buffer + received, count);
        } while (result < 0 && errno == EINTR);
        if (result < 0) {
            perror("read");
            goto out;
        }
        if (result == 0) {
            fprintf(stderr, "Early EOF after %zu of %zu bytes.\n",
                    received, length);
            goto out;
        }
        received += (size_t)result;
    }

    if (memcmp(buffer, message, length) != 0) {
        fprintf(stderr, "Read-back data does not match the message.\n");
        goto out;
    }
    do {
        result = read(fd, &extra, 1);
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        perror("read EOF");
        goto out;
    }
    if (result != 0) {
        fprintf(stderr, "Expected EOF after %zu bytes.\n", received);
        goto out;
    }

    printf("Read %zu bytes: ", received);
    fwrite(buffer, 1, received, stdout);
    putchar('\n');
    puts("PASS: read-back matches; EOF verified.");
    status = EXIT_SUCCESS;

out:
    /* Do not retry close() on Linux: the descriptor may already be closed. */
    if (close(fd) < 0) {
        perror("close");
        status = EXIT_FAILURE;
    }
    return status;
}
