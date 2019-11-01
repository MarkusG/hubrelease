make:
	gcc src/*.c -lgit2 -lcurl
debug:
	gcc -g src/*.c -lgit2 -lcurl
