#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
    int fd1, fd2, fd3;
    int oflags = O_CREAT | O_WRONLY | O_APPEND;
    int mode = S_IRUSR | S_IWUSR;
    fd1 = open(argv[1], O_RDONLY);
    fd2 = open(argv[2], O_RDONLY);
    fd3 = open(outfile, oflags, mode);
    if ((fd1 == -1) || (fd2 == -1) || (fd3 == -1)) {
        perror("open");
        exit(1);
    }
    char buff[1024];
    ssize_t rcnt, wcnt;
    size_t len, idx = 0;
    for (;;) {
        rcnt = read(fd1, buff, sizeof(buff) - 1);
        if (rcnt == 0) /* end-of-file */
            break;
        if (rcnt == -1) { /* error */
            perror("read");
            return 1;
        }
        buff[rcnt] = '\0';
        len = strlen(buff);
        do {
            wcnt = write(fd3, buff + idx, len - idx);
            if (wcnt == -1) { /* error */
                perror("write");
                return 1;
            }
            idx += wcnt;
        } while (idx < len);
    }
    idx = 0;
    for (;;) {
        rcnt = read(fd2, buff, sizeof(buff) - 1);
        if (rcnt == 0) /* end-of-file */
            return 0;
        if (rcnt == -1) { /* error */
            perror("read");
            return 1;
        }
        buff[rcnt] = '\0';
        len = strlen(buff);
        do {
            wcnt = write(fd3, buff + idx, len - idx);
            if (wcnt == -1) { /* error */
                perror("write");
                return 1;
            }
            idx += wcnt;
        } while (idx < len);
    }
    close(fd1);
    close(fd2);
    close(fd3);
    return 0;
}
