CC		:= gcc
CFLAGS	:= -Wall -g
LFLAGS	:= -lpthread

OBJS	+= XWorkQueue.o main.o
TARGET	:= wq

.PHONY: clean

all:	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LFLAGS)

clean:
	rm -rf $(OBJS) $(TARGET)
