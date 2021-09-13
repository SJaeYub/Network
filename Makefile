CC=gcc
CFLAGS=-g 
OBJS=myserver.c
TARGER=myserver

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

myserver: myserver.c