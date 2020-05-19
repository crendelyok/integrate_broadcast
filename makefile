CFLAGS  =   -std=c99 -g -Wall -Wextra
LOADLIBES = -pthread -lm

.PHONY: all

all: calc server
#server.out:
#	gcc -Wall -Wextra server.c -o server.out -lm -pthread

#worker.out:
#	gcc -Wall -Wextra calc.c -o worker.out -lm -pthread

#a.out: $(patsubst %.c,%.o,$(wildcard *.c))
#	gcc -Wall -Wextra $^ -o $@ -lm -pthread

refresh:
	rm calc server  
