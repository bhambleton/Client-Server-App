all: ftserver

ftserver: ftserver.c
	gcc -o ftserver ftserver.c
