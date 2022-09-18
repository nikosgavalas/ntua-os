#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "proc-common.h"
#include "tree.h"

void create(struct tree_node *root, int fd);

int main(int argc, char *argv[]) {
    struct tree_node *root;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_tree_file>\n\n", argv[0]);
        exit(1);
    }

    root = get_tree_from_file(argv[1]);

    if (root != NULL) {
        print_tree(root);
        printf("\n");

        pid_t pid;
        int status;
        int pfd[2];

        printf("Parent Father: Creating pipe...\n");
        if (pipe(pfd) < 0) {
            perror("pipe");
            exit(1);
        }

        fprintf(stderr, "Parent Father, PID = %ld: Creating child...\n", (long)getpid());

        pid = fork();
        if (pid < 0) {
            perror("main: fork");
            exit(1);
        }
        if (pid == 0) {
            /* Child */
            close(pfd[0]);
            change_pname(root->name);
            create(root, pfd[1]);
            close(pfd[1]);
            exit(100);
        }

        close(pfd[1]);
        sleep(3);

        show_pstree(pid);

        wait(&status);
        explain_wait_status(pid, status);

        int val;

        if (read(pfd[0], &val, sizeof(val)) != sizeof(val)) {
            perror("Father: read from pipe");
            exit(1);
        }
        printf("Father: received value from the pipe! The result is: %d\n", val);
        close(pfd[0]);

    } else
        printf("The input tree file is empty!\n");

    return 1;
}

void create(struct tree_node *root, int fd) {
    int status;
    pid_t p;
    int i, val1, val2;
    int pfd1[2], pfd2[2];

    //if parent, creating pipes
    if (root->nr_children > 0) {
        printf("Parent: Creating pipe...\n");
        if (pipe(pfd1) < 0) {
            perror("pipe");
            exit(1);
        }

        printf("Parent: Creating pipe...\n");
        if (pipe(pfd2) < 0) {
            perror("pipe");
            exit(1);
        }
    }
    //creating children, recursive calls
    for (i = 0; i < root->nr_children; i++) {
        fprintf(stderr, "Parent, PID = %ld: Creating child...\n", (long)getpid());

        p = fork();
        if (p < 0) {
            /* fork failed */
            perror("fork");
            exit(1);
        }
        if (p == 0) {
            /* In child process */
            change_pname((root->children + i)->name);

            close(pfd1[0]);
            close(pfd2[0]);

            if ((root->children + i)->nr_children > 0) {
                if (0 == i)
                    create(root->children + i, pfd1[1]);
                else
                    create(root->children + i, pfd2[1]);
            } else {
                if (0 == i) {
                    int k = atoi((root->children + i)->name);
                    if (write(pfd1[1], &k, sizeof(k)) != sizeof(k)) {
                        perror("child: write to pipe");
                        exit(1);
                    }
                } else {
                    int k = atoi((root->children + i)->name);
                    if (write(pfd2[1], &k, sizeof(k)) != sizeof(k)) {
                        perror("child: write to pipe");
                        exit(1);
                    }
                }
                sleep(5);
            }
            close(pfd1[1]);
            close(pfd2[1]);
            exit(101);
        }
        printf("Parent, PID = %ld: Created child with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)p);
    }
    close(pfd1[1]);
    close(pfd2[1]);

    for (i = 0; i < root->nr_children; i++) {
        p = wait(&status);
        explain_wait_status(p, status);
    }

    //reading from pipes
    printf("Parent: My PID is %ld. Receiving an integer value.\n", (long)getpid());
    if (read(pfd1[0], &val1, sizeof(val1)) != sizeof(val1)) {
        perror("parent: read from pipe");
        exit(1);
    }
    printf("Parent %ld: received value %d from the pipe.\n", (long)getpid(), val1);

    printf("Parent: My PID is %ld. Receiving an integer value.\n", (long)getpid());
    if (read(pfd2[0], &val2, sizeof(val2)) != sizeof(val2)) {
        perror("parent: read from pipe");
        exit(1);
    }
    printf("Parent %s: received value %d from the pipe. Will now compute.\n", root->name, val2);

    //Compute
    int res;
    if (root->name[0] == '+')
        res = val1 + val2;
    else
        res = val1 * val2;

    //Write result
    if (write(fd, &res, sizeof(res)) != sizeof(res)) {
        perror("parent: write to pipe");
        exit(1);
    }

    close(pfd1[0]);
    close(pfd2[0]);

    printf("Parent %s: All done, exiting...\n", root->name);
    return;
}
