CC=gcc

make:
	$(CC) -o hubrelease src/*.c -lgit2 -lcurl -ljansson
debug:
	$(CC) -o hubrelease -g src/*.c -lgit2 -lcurl -ljansson
clean:
	rm hubrelease
