CC     = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -O2 -pthread

ferry: src/*.c src/*.h
	$(CC) $(CFLAGS) -o $@ src/*.c

test: ferry
	./run_tests.sh

clean:
	rm -f ferry logs/*.log logs/*.out
