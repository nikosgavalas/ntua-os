#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "proc-common.h"
#include "request.h"

/* Compile-time parameters. */
#define SCHED_TQ_SEC 2  /* time quantum */
#define TASK_NAME_SZ 60 /* maximum size for a task's name */

/* Global Variable */
pid_t current;

/*
 *  Queue Initialization and Functions
 */
typedef struct node_t {
    pid_t pid;
    struct node_t *next;
} node;

node *front = NULL;
node *rear = NULL;

int isEmpty(void) {
    if (front == NULL)
        return 1;
    else
        return 0;
}

void enqueue(pid_t x) {
    node *temp = (node *)malloc(sizeof(node));
    temp->pid = x;
    temp->next = NULL;
    if (front == NULL && rear == NULL) {
        front = rear = temp;
        return;
    }
    rear->next = temp;
    rear = temp;
}

pid_t dequeue(void) {
    pid_t ret = 0;
    node *temp = front;
    if (front == NULL) {
        printf("Queue is empty!\n");
        return 0;  // Error value (Program is oriented to be used for
                   // PIDs, so this is appropriate here...)
    }
    if (front == rear) {
        ret = temp->pid;
        front = rear = NULL;
    } else {
        front = front->next;
        ret = temp->pid;
    }
    free(temp);
    return ret;
}

void print_queue() {
    node *temp;
    printf("Queue values:\n");
    for (temp = front; temp != NULL; temp = temp->next) {
        printf("%d ", temp->pid);
    }
    printf("\n");
}

/*
 * SIGALRM handler
 */
static void sigalrm_handler(int signum) {
    // assert(0 && "Please fill me!");
    if (signum == SIGALRM) {
        printf("Caught SIGALRM\n");
        kill(current, SIGSTOP);
        return;
    } else {
        printf("Unknown signal caught.Continuing...\n");
        return;
    }
}

/* 
 * SIGCHLD handler
 */
static void sigchld_handler(int signum) {
    pid_t p;
    int status;
    for (;;) {
        p = waitpid(-1, &status, WUNTRACED | WNOHANG);
        if (p < 0) {
            perror("waitpid");
            exit(1);
        }
        if (p == 0)
            break;
        explain_wait_status(p, status);
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            /* A child has died */
            printf("Child with pid %d died.\n", current /*getpid()*/);
            if (isEmpty()) {
                printf("All done. Exiting...\n");
                exit(0);
            } else {
                current = dequeue();
                if (current == 0) {
                    printf("Tried to dequeue from empty queue.Exiting...");
                    exit(0);
                }
                kill(current, SIGCONT);
            }
        }
        if (WIFSTOPPED(status)) {
            /* A child has stopped due to SIGSTOP/SIGTSTP, etc */
            enqueue(current);
            current = dequeue();
            kill(current, SIGCONT);
            alarm(SCHED_TQ_SEC);
        }
    }
}

/* Install two signal handlers.
 * One for SIGCHLD, one for SIGALRM.
 * Make sure both signals are masked when one of them is running.
 */
static void
install_signal_handlers(void) {
    sigset_t sigset;
    struct sigaction sa;

    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGCHLD);
    sigaddset(&sigset, SIGALRM);
    sa.sa_mask = sigset;
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction: sigchld");
        exit(1);
    }

    sa.sa_handler = sigalrm_handler;
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
        perror("sigaction: sigalrm");
        exit(1);
    }

    /*
     * Ignore SIGPIPE, so that write()s to pipes
     * with no reader do not result in us being killed,
     * and write() returns EPIPE instead.
     */
    if (signal(SIGPIPE, SIG_IGN) < 0) {
        perror("signal: sigpipe");
        exit(1);
    }
}

/* Function that each child calls */
void child_exec(char *exec) {
    char *args[] = {exec, NULL, NULL, NULL};
    char *env[] = {NULL};
    printf("Inside child with PID=%d.\n", getpid());
    printf("Replacing with executable \"%s\"\n", exec);
    execve(exec, args, env);
    /* execve returns only if there is an error */
    perror("execve");
    exit(1);
}

int main(int argc, char *argv[]) {
    int nproc;
    /*
     * For each of argv[1] to argv[argc - 1],
     * create a new child process, add it to the process list.
     */
    if (argc < 2) {
        printf("usage: ./scheduler exec_1 exec_2 ... exec_n\n");
        exit(0);
    }
    nproc = argc - 1; /* number of proccesses goes here */
    int i;
    pid_t pids[nproc];
    for (i = 0; i < nproc; i++) {
        pids[i] = fork();
        enqueue(pids[i]);
        if (pids[i] == 0) {
            raise(SIGSTOP);
            child_exec(argv[i + 1]);
            exit(0);
        }
    }

    /* Wait for all children to raise SIGSTOP before exec()ing. */
    wait_for_ready_children(nproc);
    printf("Children processes created.\n");
    print_queue();
    current = dequeue();

    if (current == 0) {
        printf("Tried to dequeue from empty queue. Exiting...\n");
        exit(1);
    }
    /* Install SIGALRM and SIGCHLD handlers. */
    install_signal_handlers();

    kill(current, SIGCONT);
    alarm(SCHED_TQ_SEC);

    if (nproc == 0) {
        fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
        exit(1);
    }

    /* loop forever  until we exit from inside a signal handler. */
    while (pause())
        ;

    /* Unreachable */
    fprintf(stderr, "Internal error: Reached unreachable point\n");
    return 1;
}
