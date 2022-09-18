updater1: main.o zing.o zing2.o
    gcc -o zing main.o zing.o
    gcc main.o zing2.o -o zing2

zing2.o: zing2.c
    gcc -Wall -c zing2.c

main.o: main.c
    gcc -Wall -c main.c
