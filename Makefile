CC		:= gcc
CFLAGS	:= -Wall -g

OBJS	+= workqueue.o
TARGET	:= wq

.PHONY: clean

all:	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS)

clean:
	rm -rf $(OBJS) $(TARGET)
