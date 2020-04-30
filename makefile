a.out: $(patsubst %.c,%.o,$(wildcard *.c))
	gcc -Wall -Wextra -std=c99 -pthread $^ -o $@ -lm

clean:
	rm *.o 
