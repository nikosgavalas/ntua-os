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
#define SCHED_TQ_SEC 15               /* time quantum */
#define TASK_NAME_SZ 60               /* maximum size for a task's name */
#define SHELL_EXECUTABLE_NAME "shell" /* executable for shell */

/*Global Variable*/
pid_t shell_pid, killed_pid;
int ids, new_node = 0;

/*
 *  Queue Initialization and Functions
 */

typedef struct node_t {
    int id;
    pid_t pid;
    char *name;
    char *priority;
    struct node_t *next;
} node;

node current;
node *front_l = NULL, *front_h = NULL;
node *rear_l = NULL, *rear_h = NULL;

int isEmpty(void) {
    if ((front_l == NULL) && (front_h == NULL))
        return 1;
    else
        return 0;
}

void enqueue(int num, pid_t x, char *myname, char *mypriority) {
    node *temp = (node *)malloc(sizeof(node));
    temp->id = num;
    temp->pid = x;
    temp->name = myname;
    temp->priority = mypriority;
    temp->next = NULL;
    if (strcmp(mypriority, "low") == 0) {
        if (front_l == NULL && rear_l == NULL) {
            front_l = rear_l = temp;
            return;
        }
        rear_l->next = temp;
        rear_l = temp;
    } else {
        if (front_h == NULL && rear_h == NULL) {
            front_h = rear_h = temp;
            return;
        }
        rear_h->next = temp;
        rear_h = temp;
    }
}

node dequeue(char pr) {
    // pid_t ret = 0;
    node ret, *temp;
    if (pr == 'l') {
        temp = front_l;
        if (front_l == NULL) {
            printf("Queue is empty!\n");
            ret.pid = 0;
            ret.id = -1;
            ret.name = "NONE";
            ret.priority = "NONE";
            return ret;  // Error value (Program is oriented to be used for
                         // PIDs, so this is appropriate here...)
        }
        if (front_l == rear_l) {
            front_l = rear_l = NULL;
        } else {
            front_l = front_l->next;
        }
    } else {
        temp = front_h;
        if (front_h == NULL) {
            printf("Queue is empty!\n");
            ret.pid = 0;
            ret.id = -1;
            ret.name = "NONE";
            ret.priority = "NONE";
            return ret;  // Error value (Program is oriented to be used for
                         // PIDs, so this is appropriate here...)
        }
        if (front_h == rear_h) {
            front_h = rear_h = NULL;
        } else {
            front_h = front_h->next;
        }
    }
    ret.pid = temp->pid;
    ret.id = temp->id;
    ret.name = temp->name;
    ret.priority = temp->priority;

    free(temp);
    return ret;
}

void print_queue(void) {
    node *temp;
    printf("Queue values:\n");
    printf("ID:%d, PID:%d, NAME:%s, PRIORITY:%s  <-Current\n", current.id, current.pid, current.name, current.priority);
    for (temp = front_h; temp != NULL; temp = temp->next) {
        printf("ID:%d, PID:%d, NAME:%s, PRIORITY:%s\n", temp->id, temp->pid, temp->name, temp->priority);
    }
    for (temp = front_l; temp != NULL; temp = temp->next) {
        printf("ID:%d, PID:%d, NAME:%s, PRIORITY:%s\n", temp->id, temp->pid, temp->name, temp->priority);
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

/* Print a list of all tasks currently being scheduled.  */
static void
sched_print_tasks(void) {
    print_queue();
}

/* Send SIGKILL to a task determined by the value of its
 * scheduler-specific id.
 */
static int
sched_kill_task_by_id(int id) {
    node *temp, *delete, *front;
    int i;
    front = front_l;
    if ((front_l == NULL) && (front_h == NULL)) {
        printf("There is nothing to kill\n");
        return 1;
    }
    for (i = 0; i < 2; i++) {
        temp = front;
        if (temp == NULL)
            return 1;
        if ((temp->id) == id) {
            new_node = 1;
            killed_pid = temp->pid;
            front = front->next;
            if (i == 0) {
                front_l = front;
                if (front == NULL)
                    rear_l = NULL;
            }

            else {
                front_h = front;
                if (front == NULL)
                    rear_h = NULL;
            }
            kill(temp->pid, SIGKILL);
            free(temp);
            return 1;
        }
        while ((temp->next) != NULL) {
            if ((temp->next->id) == id)
                break;
            temp = temp->next;
        }
        if ((temp->next) != NULL) {
            new_node = 1;
            killed_pid = temp->next->pid;
            delete = temp->next;
            if ((temp->next->next) == NULL) {
                if (i == 0)
                    rear_l = temp;
                else
                    rear_h = temp;
            }
            kill(temp->next->pid, SIGKILL);
            temp->next = temp->next->next;
            if ((temp->next) == NULL) {
                if (i == 0)
                    rear_l = temp;
                else
                    rear_h = temp;
            }
            free(delete);
            return 1;
        }
        front = front_h;
    }
    return 1;
}

/* Create a new task.  */
static void
sched_create_task(char *executable) {
    pid_t p;
    new_node = 1;
    p = fork();
    enqueue(ids, p, executable, "low");
    ids++;
    if (p == 0) {
        raise(SIGSTOP);
        child_exec(executable);
        exit(0);
    }
}

static void
sched_high_priority(int id) {
    node *temp, *delete;
    temp = front_l;
    if (front_l == NULL) {
        printf("No low priority process\n");
        return;
    }
    if ((temp->id) == id) {
        enqueue(temp->id, temp->pid, temp->name, "high");
        front_l = front_l->next;
        if (front_l == NULL)
            rear_l = NULL;
        free(temp);
        return;
    }
    while ((temp->next) != NULL) {
        if ((temp->next->id) == id)
            break;
        temp = temp->next;
    }
    if ((temp->next) != NULL) {
        enqueue(temp->next->id, temp->next->pid, temp->next->name, "high");
        delete = temp->next;
        if ((temp->next->next) == NULL)
            rear_l = temp;
        temp->next = temp->next->next;
        free(delete);
        if (temp->next == NULL)
            rear_l = temp;
    }

    return;
}

static void
sched_low_priority(int id) {
    node *temp, *delete;
    temp = front_h;
    if (front_h == NULL) {
        printf("No high priority process\n");
        return;
    }
    if ((temp->id) == id) {
        enqueue(temp->id, temp->pid, temp->name, "low");
        front_h = front_h->next;
        if (front_h == NULL)
            rear_h = NULL;
        free(temp);
        return;
    }
    while ((temp->next) != NULL) {
        if ((temp->next->id) == id)
            break;
        temp = temp->next;
    }
    if ((temp->next) != NULL) {
        enqueue(temp->next->id, temp->next->pid, temp->next->name, "low");
        delete = temp->next;
        if ((temp->next->next) == NULL)
            rear_h = temp;
        temp->next = temp->next->next;
        free(delete);
        if (temp->next == NULL)
            rear_h = temp;
    }
    return;
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

        case REQ_HIGH_TASK:
            sched_high_priority(rq->task_arg);
            return 0;

        case REQ_LOW_TASK:
            sched_low_priority(rq->task_arg);
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
    if (signum == SIGALRM) {
        printf("Caught SIGALRM\n");
        kill(current.pid, SIGSTOP);
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
    node *temp;
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
            temp = front_l;
            if (new_node == 1) {
                new_node = 0;
                printf("Child with pid %d died.\n", killed_pid);
            } else {
                while (temp != NULL) {
                    if (temp->id == 0)
                        break;
                    temp = temp->next;
                }

                if ((temp == NULL) && (shell_pid != 0)) {
                    printf("Child with pid %d died.\n", shell_pid);
                    shell_pid = 0;
                } else
                    printf("Child with pid %d died.\n", current.pid);
                if (isEmpty() /*&& shell_pid == 0*/) {
                    printf("All done. Exiting...\n");
                    exit(0);
                } else {
                    if (front_h != NULL)
                        current = dequeue('h');
                    else
                        current = dequeue('l');
                    if (current.pid == 0) {
                        printf("Tried to dequeue from empty queue.Exiting...");
                        exit(0);
                    }
                    kill(current.pid, SIGCONT);
                }
            }
        }
        if (WIFSTOPPED(status)) {
            /* A child has stopped due to SIGSTOP/SIGTSTP, etc */
            if (new_node == 1)
                new_node = 0;
            else {
                enqueue(current.id, current.pid, current.name, current.priority);
                if (front_h != NULL)
                    current = dequeue('h');
                else
                    current = dequeue('l');
                kill(current.pid, SIGCONT);
                alarm(SCHED_TQ_SEC);
            }
        }
    }
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

    p = fork();
    shell_pid = p;
    if (p < 0) {
        perror("scheduler: fork");
        exit(1);
    }

    if (p == 0) {
        /* Child */
        close(pfds_rq[0]);
        close(pfds_ret[1]);
        do_shell(executable, pfds_rq[1], pfds_ret[0]);
    }
    /* Parent */
    enqueue(ids, p, "shell", "low");
    ids++;
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
    /* Two file descriptors for communication with the shell */
    static int request_fd, return_fd;

    /* Create the shell. */
    sched_create_shell(SHELL_EXECUTABLE_NAME, &request_fd, &return_fd);
    /* TODO: add the shell to the scheduler's tasks */

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
        enqueue(ids, pids[i], argv[i + 1], "low");
        ids++;
        if (pids[i] == 0) {
            raise(SIGSTOP);
            child_exec(argv[i + 1]);
            exit(0);
        }
    }

    /* Wait for all children to raise SIGSTOP before exec()ing. */
    wait_for_ready_children(nproc + 1);
    printf("Children processes created.\n");

    current = dequeue('l');

    if (current.pid == 0) {
        printf("Tried to dequeue from empty queue. Exiting...\n");
        exit(1);
    }

    /* Install SIGALRM and SIGCHLD handlers. */
    install_signal_handlers();

    kill(current.pid, SIGCONT);
    alarm(SCHED_TQ_SEC);

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
