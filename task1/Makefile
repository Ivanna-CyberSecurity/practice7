CC = gcc
CFLAGS = -Wall

all: sender receiver

sender: sender.c
	$(CC) $(CFLAGS) sender.c -o sender

receiver: receiver.c
	$(CC) $(CFLAGS) receiver.c -o receiver

.PHONY: clean

clean:
	rm -f sender receiver
