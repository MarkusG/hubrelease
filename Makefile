CC=gcc

make:
	$(CC) -o releaser src/*.c -lgit2 -lcurl -ljansson
debug:
	$(CC) -o releaser -g src/*.c -lgit2 -lcurl -ljansson
clean:
	rm releaser
