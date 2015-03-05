test_server2:
		gcc -g -o test_server2 lib_net.h lib_net.c lib_thread.h lib_thread.c test_server2.c -levent -lpthread

clean:
		rm test_server2
