# CC = gcc
# CXX = g++
# CFLAGS = -Wall -g 
# LDFLAGS = -lpthread -levent
# TARGET: test_client test_server3
# all: $(TARGET)
# #all: test_client test_sever3
# test_client:
# 		gcc -g -o test_client lib_net.h  lib_net.c lib_public.h lib_public.c test_client.c 
# test_server3:
# 		gcc -g -o test_server3 lib_net.h lib_net.c lib_thread.h lib_thread.c lib_public.h lib_public.c lib_file.h lib_file.c test_server3.c -levent -lpthread
# # test_client: lib_net.o lib_public.o test_client.o
# # 		$(CC) -o $@ $^ 

# # test_server3: lib_net.o lib_thread.o lib_public.o lib_file.o test_server3.o
# # 		$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
# # .c.o:
# # 		$(CC) -c $< 
# clean:
# 		rm -rf *.o test_client test_server3

CC = gcc  
CXX = g++  
CFLAGS = -O -g
LDFLAGS = -lpthread -levent 

SRCS = $(wildcard *.c)  
OBJS = $(patsubst %c, %o, $(SRCS))

TARGET = testserver testclient  
all: $(TARGET)  
IGN1 = test_client.o test_server2.o test_server.o
IGN2 = test_server3.o test_server2.o test_server.o

testserver : test_server3.o $(filter-out $(IGN1),$(OBJS))
		$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) 
testclient :  test_client.o $(filter-out $(IGN2),$(OBJS))  
		$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)   


%.o : %.c  
		$(CC) $(CFLAGS) -c $^ -o $@  
# %.o : %.cpp  
# 		$(CXX) $(CFLAGS) -c $^ -o $@   


.PHONY : clean  

clean :  
		rm -f *.o  
		rm -f testclient testserver

# install:  
#       mv Excute excute; cp -f ordermisd ../bin/;  
