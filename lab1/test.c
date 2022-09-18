#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    char buff[1];
    ssize_t rcnt;
    for (;;) {
        rcnt = read(0, buff, 1);
        if (rcnt == 0) /* end-of-file */
            return 0;
        if (rcnt == -1) { /* error */
            perror("read");
            return 1;
        }
        buff[rcnt + 2000] = '\0';
        fprintf(stdout, "%s", buff);
        fflush(stdout);
    }
}
