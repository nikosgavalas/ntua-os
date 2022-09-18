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

        wait_for_ready_children(1);

        show_pstree(pid);

        kill(pid, SIGCONT);

        wait(&status);
        explain_wait_status(pid, status);
    } else
        printf("The input tree file is empty!\n");

    return 1;
}

void create(struct tree_node *root) {
    int status;
    pid_t A[200];
    int i;

    for (i = 0; i < root->nr_children; i++) {
        fprintf(stderr, "Parent, PID = %ld: Creating child...\n", (long)getpid());

        A[i] = fork();
        if (A[i] < 0) {
            /* fork failed */
            perror("fork");
            exit(1);
        }
        if (A[i] == 0) {
            /* In child process */
            change_pname((root->children + i)->name);

            create(root->children + i);

            exit(101);
        }
        printf("Parent, PID = %ld: Created child with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)A[i]);
    }

    wait_for_ready_children(root->nr_children);
    raise(SIGSTOP);
    printf("PID = %ld, name = %s is awake\n", (long)getpid(), root->name);

    for (i = 0; i < root->nr_children; i++) {
        kill(A[i], SIGCONT);
        wait(&status);
        explain_wait_status(A[i], status);
    }

    return;
}
