#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "proc-common.h"
#include "tree.h"

void create(struct tree_node *root);

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

        fprintf(stderr, "Parent Father, PID = %ld: Creating child...\n", (long)getpid());

        pid = fork();
        if (pid < 0) {
            perror("main: fork");
            exit(1);
        }
        if (pid == 0) {
            /* Child */
            change_pname(root->name);
            create(root);
            exit(101);
        }

        sleep(3);

        show_pstree(pid);

        wait(&status);
        explain_wait_status(pid, status);
    } else
        printf("The input tree file is empty!\n");

    return 1;
}

void create(struct tree_node *root) {
    int status;
    pid_t p;
    int i;

    for (i = 0; i < root->nr_children; i++) {
        fprintf(stderr, "Parent, PID = %ld: Creating child...\n",
                (long)getpid());

        p = fork();
        if (p < 0) {
            /* fork failed */
            perror("fork");
            exit(1);
        }
        if (p == 0) {
            /* In child process */
            change_pname((root->children + i)->name);

            if ((root->children + i)->nr_children > 0)
                create(root->children + i);
            else
                sleep(5);

            exit(101);
        }

        printf("Parent, PID = %ld: Created child with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)p);
    }

    for (i = 0; i < root->nr_children; i++) {
        p = wait(&status);
        explain_wait_status(p, status);
    }

    printf("Parent %s: All done, exiting...\n", root->name);

    return;
}
