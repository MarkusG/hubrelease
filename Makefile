make:
	gcc src/*.c -lgit2 -lcurl -ljansson
debug:
	gcc -g src/*.c -lgit2 -lcurl -ljansson
