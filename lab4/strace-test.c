/*
 * strace-test.c
 *
 * Make sure everything works OK when strace'ing
 * a program using fork() and signals.
 *
 * Usage:
 *
 * strace -f ./strace-test 2>/dev/null
 *
 */

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define STOP_SIGNAL SIGTSTP

int main(int argc, char *argv[]) {
    int status;
    pid_t p, waitp;

    p = fork();
    if (p < 0) {
        perror("fork");
        exit(1);
    }

    if (p == 0) {
        raise(STOP_SIGNAL);
        /* Should never reach this point */
        printf("Child: FAIL.\n");
        exit(1);
    }

    /*
     * Father sleeps for a while and kills the child.
     * The child should be alive when the father wakes.
     */
    printf("Parent: sleeping for a while...\n");
    sleep(2);

    if (kill(p, SIGKILL) < 0) {
        perror("kill");
        exit(1);
    }

    waitp = wait(&status);
    assert(p == waitp);
    if (WIFEXITED(status)) {
        printf("Parent: FAIL. Child exited, status = %d\n",
               WEXITSTATUS(status));
        exit(1);
    }

    if (!WIFSIGNALED(status)) {
        printf("Parent: FAIL. Child not signaled?\n");
        exit(1);
    }

    printf("Parent: SUCCESS. Child killed, sig = %d\n",
           WTERMSIG(status));
    return 0;
}
