a.out: $(patsubst %.c,%.o,$(wildcard *.c))
	gcc -Wall -Wextra -pthread $^ -o $@ -lm

clean:
	rm *.o 
