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
#define SCHED_TQ_SEC 10               /* time quantum */
#define TASK_NAME_SZ 60               /* maximum size for a task's name */
#define SHELL_EXECUTABLE_NAME "shell" /* executable for shell */

/* Global Variable */
struct info_t current;
int global_id = 1;

/*
 *  Queue Initialization and Functions
 */

struct info_t {
    int id;
    pid_t pid;
    char *name;
};

typedef struct node_t {
    struct info_t info;
    struct node_t *next;
} node;

struct queue_t {
    node *front;
    node *rear;
};

struct queue_t queue_obj1;
struct queue_t queue_obj2;
struct queue_t *queue_low;
struct queue_t *queue_high;

int isEmpty(struct queue_t *Queue) {
    return (Queue->front == NULL ? 1 : 0);
}

void enqueue(struct queue_t *Queue, struct info_t info_arg) {
    node *temp = (node *)malloc(sizeof(node));
    temp->info.id = info_arg.id;
    temp->info.pid = info_arg.pid;
    temp->info.name = info_arg.name;
    temp->next = NULL;
    if (Queue->front == NULL && Queue->rear == NULL) {
        printf(">>>>>>>>>>\n");
        Queue->front = temp;
        Queue->rear = temp;
        return;
    }
    printf("<<<<<<<<<<\n");
    Queue->rear->next = temp;
    Queue->rear = temp;
    return;
}

struct info_t dequeue(struct queue_t *Queue) {
    struct info_t ret;
    node *temp = Queue->front;
    if (Queue->front == NULL) {
        printf("Queue is empty!\n");
        ret.id = 0;
        ret.pid = 0;
        ret.name = "ERROR";
        return ret;  // Error value (Program is oriented to be used for
                     // PIDs, so this is appropriate here)
    }
    if (Queue->front == Queue->rear) {
        ret.pid = temp->info.pid;
        ret.id = temp->info.id;
        ret.name = temp->info.name;
        Queue->front = Queue->rear = NULL;
    } else {
        Queue->front = Queue->front->next;
        ret.pid = temp->info.pid;
        ret.id = temp->info.id;
        ret.name = temp->info.name;
    }
    free(temp);
    return ret;
}

void print_queue(struct queue_t *Queue) {
    node *temp;
    for (temp = Queue->front; temp != NULL; temp = temp->next) {
        printf("PID:%d id:%d name:%s\n", temp->info.pid, temp->info.id, temp->info.name);
    }
    printf("\n");
}

/* Print a list of all tasks currently being scheduled.  */
static void
sched_print_tasks(void) {
    // assert(0 && "Please fill me!");
    if (!isEmpty(queue_high)) {
        printf("High priority tasks: \n");
        print_queue(queue_high);
    }
    printf("Low priority tasks :\n");
    print_queue(queue_low);
    return;
}

/* Send SIGKILL to a task determined by the value of its
 * scheduler-specific id.
 */
static int
sched_kill_task_by_id(int id) {
    assert(0 && "Please fill me!");
    return -ENOSYS;
}

/* Create a new task.  */
static void
sched_create_task(char *executable) {
    assert(0 && "Please fill me!");
}

/* Process requests by the shell.  */
static int
process_request(struct request_struct *rq) {
    switch (rq->request_no) {
        case REQ_PRINT_TASKS:
            sched_print_tasks();
            return 0;

        case REQ_KILL_TASK:
            return sched_kill_task_by_id(rq->task_arg);

        case REQ_EXEC_TASK:
            sched_create_task(rq->exec_task_arg);
            return 0;

        default:
            return -ENOSYS;
    }
}

/* 
 * SIGALRM handler
 */
static void
sigalrm_handler(int signum) {
    // assert(0 && "Please fill me!");
    if (signum == SIGALRM) {
        printf("Caught SIGALRM\n");
        kill(current.pid, SIGSTOP);
        // enqueue(current);
        // current = dequeue();
        // kill(current, SIGCONT);
        // alarm(SCHED_TQ_SEC);
        return;
    } else {
        printf("Unknown signal caught.Continuing...\n");
        return;
    }
}

/* 
 * SIGCHLD handler
 */
static void
sigchld_handler(int signum) {
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
            printf("Child with pid %d died.\n", getpid());
            if (isEmpty(queue_low)) {
                printf("All done. Exiting...\n");
                exit(0);
            } else {
                current.pid = dequeue(queue_low).pid;
                if (current.pid == 0) {
                    printf("Tried to dequeue from empty queue.Exiting...");
                    exit(0);
                }
                kill(current.pid, SIGCONT);
            }
        }
        if (WIFSTOPPED(status)) {
            /* A child has stopped due to SIGSTOP/SIGTSTP, etc */
            enqueue(queue_low, current);
            current.pid = dequeue(queue_low).pid;
            kill(current.pid, SIGCONT);
            alarm(SCHED_TQ_SEC);
        }
    }
}

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

/* Disable delivery of SIGALRM and SIGCHLD. */
static void
signals_disable(void) {
    sigset_t sigset;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    sigaddset(&sigset, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
        perror("signals_disable: sigprocmask");
        exit(1);
    }
}

/* Enable delivery of SIGALRM and SIGCHLD.  */
static void
signals_enable(void) {
    sigset_t sigset;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    sigaddset(&sigset, SIGCHLD);
    if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) < 0) {
        perror("signals_enable: sigprocmask");
        exit(1);
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

static void
do_shell(char *executable, int wfd, int rfd) {
    char arg1[10], arg2[10];
    char *newargv[] = {executable, NULL, NULL, NULL};
    char *newenviron[] = {NULL};

    sprintf(arg1, "%05d", wfd);
    sprintf(arg2, "%05d", rfd);
    newargv[1] = arg1;
    newargv[2] = arg2;

    raise(SIGSTOP);
    execve(executable, newargv, newenviron);

    /* execve() only returns on error */
    perror("scheduler: child: execve");
    exit(1);
}

/* Create a new shell task.
 *
 * The shell gets special treatment:
 * two pipes are created for communication and passed
 * as command-line arguments to the executable.
 */
static void
sched_create_shell(char *executable, int *request_fd, int *return_fd) {
    pid_t p;
    int pfds_rq[2], pfds_ret[2];

    if (pipe(pfds_rq) < 0 || pipe(pfds_ret) < 0) {
        perror("pipe");
        exit(1);
    }
    /* Creating the Shell */
    p = fork();
    if (p < 0) {
        perror("scheduler: fork");
        exit(1);
    }

    if (p == 0) {
        /* Child */
        close(pfds_rq[0]);
        close(pfds_ret[1]);
        do_shell(executable, pfds_rq[1], pfds_ret[0]);
        assert(0);
    }
    /* Parent */

    /* Enqueueing the Shell */
    printf("Created the shell with PID:%ld ...\n", (long)p);

    struct info_t info;
    info.id = global_id++;
    info.pid = p;
    info.name = "Shell";
    enqueue(queue_low, info);

    close(pfds_rq[1]);
    close(pfds_ret[0]);
    *request_fd = pfds_rq[0];
    *return_fd = pfds_ret[1];
}

static void
shell_request_loop(int request_fd, int return_fd) {
    int ret;
    struct request_struct rq;

    /*
     * Keep receiving requests from the shell.
     */
    for (;;) {
        if (read(request_fd, &rq, sizeof(rq)) != sizeof(rq)) {
            perror("scheduler: read from shell");
            fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
            break;
        }

        signals_disable();
        ret = process_request(&rq);
        signals_enable();

        if (write(return_fd, &ret, sizeof(ret)) != sizeof(ret)) {
            perror("scheduler: write to shell");
            fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    int nproc;
    queue_low = &queue_obj1;
    queue_high = &queue_obj2;
    queue_high->front = NULL;
    queue_high->rear = NULL;
    queue_low->front = NULL;
    queue_high->rear = NULL;
    /* Two file descriptors for communication with the shell */
    static int request_fd, return_fd;

    /* Create the shell. */
    sched_create_shell(SHELL_EXECUTABLE_NAME, &request_fd, &return_fd);
    /* TODO: add the shell to the scheduler's tasks */

    /*
     * For each of argv[1] to argv[argc - 1],
     * create a new child process, add it to the process list.
     */

    nproc = argc - 1; /* number of proccesses goes here */
    int i;
    struct info_t info[nproc];
    for (i = 0; i < nproc; i++) {
        info[i].pid = fork();
        info[i].id = global_id++;
        info[i].name = argv[i + 1];
        enqueue(queue_low, info[i]);
        if (info[i].pid == 0) {
            raise(SIGSTOP);
            child_exec(argv[i + 1]);
            exit(0);
        }
    }

    /* Wait for all children to raise SIGSTOP before exec()ing. */
    wait_for_ready_children(nproc + 1);  // nproc + 1 because parent needs to wait for the shell also

    /* Install SIGALRM and SIGCHLD handlers. */
    install_signal_handlers();

    current.pid = dequeue(queue_low).pid;
    kill(current.pid, SIGCONT);

    if (alarm(SCHED_TQ_SEC) < 0) {
        perror("alarm");
        exit(1);
    }

    if (nproc == 0) {
        fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
        exit(1);
    }

    shell_request_loop(request_fd, return_fd);

    /* Now that the shell is gone, just loop forever
     * until we exit from inside a signal handler.
     */
    while (pause())
        ;

    /* Unreachable */
    fprintf(stderr, "Internal error: Reached unreachable point\n");
    return 1;
}
