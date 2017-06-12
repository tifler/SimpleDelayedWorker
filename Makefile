CC		:= gcc
CFLAGS	:= -Wall -g
LFLAGS	:= -lpthread

OBJS	+= workqueue.o
TARGET	:= wq

.PHONY: clean

all:	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LFLAGS)

clean:
	rm -rf $(OBJS) $(TARGET)
