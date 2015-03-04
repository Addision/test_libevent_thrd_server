test_server:
		gcc -o test_server lib_net.h lib_net.c lib_thread.h lib_thread.c test_server.c -levent -lpthread

clean:
		rm test_server 
