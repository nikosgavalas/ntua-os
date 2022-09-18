#include <stdio.h>
#include <unistd.h>

void zing() {
    printf("How are you %s?\n", getlogin());
}
