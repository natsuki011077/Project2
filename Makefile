OBJS1 = sender.o
OBJS2 = receiver.o
CC = g++

all: sender receiver clean

sender: $(OBJS1)
	$(CC) $(OBJS1) -o $@
receiver : $(OBJS2)
	$(CC) $(OBJS2) -o $@

sender.o: sender.cpp header.h
	$(CC) -c sender.cpp

receiver.o: receiver.cpp header.h
	$(CC) -c receiver.cpp

clean:
	\rm -f *.o
