# Jessica Seabolt 11/17/2023 CMP_SCI 4760 Project 6
CC = gcc
CFLAGS = -Wall -g -std=gnu99

all: oss user_proc

oss: oss.o
	$(CC) $(CFLAGS) -o oss oss.o

user_proc: user_proc.o
	$(CC) $(CFLAGS) -o user_proc user_proc.o

oss.o: oss.c header.h
	$(CC) $(CFLAGS) -c oss.c

user_proc.o: user_proc.c header.h
	$(CC) $(CFLAGS) -c user_proc.c

clean:
	rm -f *.o oss user_proc