/*
 * mandel.c
 *
 * A program to draw the Mandelbrot Set on a 256-color xterm.
 *
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include "mandel-lib.h"

#define MANDEL_MAX_ITERATION 100000

#define perror_pthread(ret, msg) \
    do {                         \
        errno = ret;             \
        perror(msg);             \
    } while (0)

/***************************
 * Compile-time parameters *
 ***************************/

int NTHREADS;

/*
 * Output at the terminal is is x_chars wide by y_chars long
*/
int y_chars = 50;
int x_chars = 90;

/*
 * The part of the complex plane to be drawn:
 * upper left corner is (xmin, ymax), lower right corner is (xmax, ymin)
*/
double xmin = -1.8, xmax = 1.0;
double ymin = -1.0, ymax = 1.0;

/*
 * Every character in the final output is
 * xstep x ystep units wide on the complex plane.
 */
double xstep;
double ystep;

struct thread_info_struct {
    pthread_t tid;
    sem_t sem;
};

struct thread_info_struct *thr;

/*
 * This function computes a line of output
 * as an array of x_char color values.
 */
void compute_mandel_line(int line, int color_val[]) {
    /*
     * x and y traverse the complex plane.
     */
    double x, y;

    int n;
    int val;

    /* Find out the y value corresponding to this line */
    y = ymax - ystep * line;

    /* and iterate for all points on this line */
    for (x = xmin, n = 0; n < x_chars; x += xstep, n++) {
        /* Compute the point's color value */
        val = mandel_iterations_at_point(x, y, MANDEL_MAX_ITERATION);
        if (val > 255)
            val = 255;

        /* And store it in the color_val[] array */
        val = xterm_color(val);
        color_val[n] = val;
    }
}

/*
 * This function outputs an array of x_char color values
 * to a 256-color xterm.
 */
void output_mandel_line(int fd, int color_val[]) {
    int i;

    char point = '@';
    char newline = '\n';

    for (i = 0; i < x_chars; i++) {
        /* Set the current color, then output the point */
        set_xterm_color(fd, color_val[i]);
        if (write(fd, &point, 1) != 1) {
            perror("compute_and_output_mandel_line: write point");
            exit(1);
        }
    }

    /* Now that the line is done, output a newline character */
    if (write(fd, &newline, 1) != 1) {
        perror("compute_and_output_mandel_line: write newline");
        exit(1);
    }
}

void compute_and_output_mandel_line(int fd, int line) {
    /*
     * A temporary array, used to hold color values for the line being drawn
     */
    int color_val[x_chars];

    compute_mandel_line(line, color_val);
    output_mandel_line(fd, color_val);
}

void *thread_fn(void *i) {
    int color_val[x_chars];
    int fd = 1;
    int line, j = (int)i;

    for (line = j; line < y_chars; line = line + NTHREADS) {
        compute_mandel_line(line, color_val);

        sem_wait(&thr[j].sem);

        output_mandel_line(fd, color_val);

        if ((j + 1) < NTHREADS) {
            sem_post(&thr[j + 1].sem);
        } else {
            sem_post(&thr[0].sem);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int ret, i;

    if (argc != 2) {
        printf("Exactly one argument required: the number of threads to create.\n");
        exit(1);
    }

    NTHREADS = atoi(argv[1]);

    if (NTHREADS < 1 || NTHREADS > 50) {
        fprintf(stderr, "%d must be less than 50 and over 0 \n", NTHREADS);
        exit(1);
    }

    thr = malloc(NTHREADS * sizeof(struct thread_info_struct));

    if (sem_init(&thr[0].sem, 0, 1) == -1)
        exit(1);
    for (i = 1; i < NTHREADS; i++) {
        if (sem_init(&thr[i].sem, 0, 0) == -1)
            exit(1);
    }

    xstep = (xmax - xmin) / x_chars;
    ystep = (ymax - ymin) / y_chars;

    for (i = 0; i < NTHREADS; i++) {
        ret = pthread_create(&thr[i].tid, NULL, thread_fn, (void *)i);
        if (ret) {
            perror_pthread(ret, "pthread_create");
            exit(1);
        }
    }

    for (i = 0; i < NTHREADS; i++) {
        ret = pthread_join(thr[i].tid, NULL);
        if (ret) {
            perror_pthread(ret, "pthread_join");
            exit(1);
        }
    }

    reset_xterm_color(1);
    return 0;
}
