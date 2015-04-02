CC = gcc
CXX = g++
CFLAGS = -Wall -g -o2
LDFLAGS = -lpthread -levent
TARGET: test_client test_server2
all: $(TARGET)
#test_client:\
		gcc -g -o test_client lib_net.h  lib_net.c lib_public.h lib_public.c test_client.c 
#test_server2:\
		gcc -g -o test_server2 lib_net.h lib_net.c lib_thread.h lib_thread.c lib_public.h lib_public.c lib_file.h lib_file.c test_server2.c -levent -lpthread
test_client: lib_net.o lib_public.o test_client.o
		$(CC) -o $@ $^ 

test_server2: lib_net.o lib_thread.o lib_public.o lib_file.o test_server2.o
		$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
.c.o:
		$(CC) -c $< 
clean:
		rm *.o test_server2 test_client 
