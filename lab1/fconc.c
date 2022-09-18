#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

size_t idx = 0;

void doWrite(int fd, const char *buff, int len) {
    ssize_t wcnt;
    do {
        wcnt = write(fd, buff + idx, len - idx);
        if (wcnt == -1) { /*error*/
            perror("write");
            exit(1);
        }
        idx += wcnt;
    } while (idx < len);
}

void write_file(int fd, const char *infile) {
    int fd1;
    fd1 = open(infile, O_RDONLY);
    if (fd1 == -1) {
        perror("open");
        exit(1);
    }
    char buff[1024];
    ssize_t rcnt;
    size_t len;
    idx = 0;
    for (;;) {
        rcnt = read(fd1, buff, sizeof(buff) - 1);
        if (rcnt == 0) /*end of file*/
            break;
        if (rcnt == -1) { /*error*/
            perror("read");
            exit(1);
        }
        buff[rcnt] = '\0';
        len = strlen(buff);
        doWrite(fd, buff, len);
    }
    close(fd1);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: ./fconc infile1 infile2 [ outfile (default: fconc.out ) ]\n");
        return -1;
    }
    char *outfile;
    if (argc == 3) {
        outfile = "fconc.out";
    }
    if (argc == 4) {
        outfile = argv[3];
    }
    int fd;
    int oflags = O_CREAT | O_WRONLY | O_APPEND;
    int mode = S_IRUSR | S_IWUSR;
    fd = open(outfile, oflags, mode);
    if (fd == -1) {
        perror("open");
        exit(1);
    }
    write_file(fd, argv[1]);
    write_file(fd, argv[2]);
    close(fd);
    return 0;
}
