#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "proc-common.h"

#define SLEEP_SEC 5
#define SLEEP_LESS 3

int main(void) {
    pid_t p;
    int status;

    fprintf(stderr, "TreeRoot, PID = %ld: Creating A...\n", (long)getpid());

    p = fork();
    if (p < 0) {
        /* fork failed */
        perror("fork");
        exit(1);
    }
    if (p == 0) {
        /* In child process A*/
        change_pname("A");

        fprintf(stderr, "A, PID = %ld: Creating B...\n", (long)getpid());

        p = fork();
        if (p < 0) {
            /* fork failed */
            perror("fork");
            exit(1);
        }
        if (p == 0) {
            /* In child process B*/
            change_pname("B");

            fprintf(stderr, "B, PID = %ld: Creating D...\n", (long)getpid());

            p = fork();
            if (p < 0) {
                /* fork failed */
                perror("fork");
                exit(1);
            }
            if (p == 0) {
                /* In child process D */
                change_pname("D");

                printf("D: Sleeping...\n");
                sleep(SLEEP_SEC);

                printf("D: Waking up..\n");
                exit(13);
            }
            /* Back in B */

            printf("B, PID = %ld: Created D with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)p);

            p = wait(&status);
            explain_wait_status(p, status);

            printf("B: All done, exiting...\n");
            exit(19);
        }
        /*Back in A */

        printf("A, PID = %ld: Created B with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)p);

        fprintf(stderr, "A, PID = %ld: Creating C...\n", (long)getpid());

        p = fork();
        if (p < 0) {
            /* fork failed */
            perror("fork");
            exit(1);
        }
        if (p == 0) {
            /* In child process C */
            change_pname("C");

            printf("C: Sleeping...\n");
            sleep(SLEEP_SEC);

            printf("C: Waking up..\n");
            exit(17);
        }
        /* Back in A */

        printf("A, PID = %ld: Created C with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)p);

        //wait for both B and C
        p = wait(&status);
        explain_wait_status(p, status);

        p = wait(&status);
        explain_wait_status(p, status);

        printf("Parent A: All done, exiting...\n");
        exit(16);
    }
    /* Back in TreeRoot */

    change_pname("TreeRoot");

    printf("TreeRoot, PID = %ld: Created A with PID = %ld, waiting for it to terminate...\n", (long)getpid(), (long)p);

    sleep(SLEEP_LESS);

    show_pstree(p);

    p = wait(&status);
    explain_wait_status(p, status);

    printf("TreeRoot: All done, exiting...\n");

    return 0;
}
