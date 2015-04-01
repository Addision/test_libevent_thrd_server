all:test_client test_server2
test_client:
		gcc -g -o test_client lib_net.h  lib_net.c lib_public.h lib_public.c test_client.c 
test_server2:
		gcc -g -o test_server2 lib_net.h lib_net.c lib_thread.h lib_thread.c lib_public.h lib_public.c test_server2.c -levent -lpthread
clean:
		rm test_server2 test_client 
