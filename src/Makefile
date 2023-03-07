
test-bench:wlmalloc.c test-bench.c
	gcc -g -o test-bench.out wlmalloc.c test-bench.c -lpthread

test:wlmalloc.c test.c
	gcc -g -o test.out wlmalloc.c test.c -lpthread

ls:wlmalloc.c ls.c
	gcc -lpthread -o ls.out ls.c wlmalloc.c

lib:
	gcc -fpic -shared -o libwlmalloc.so wlmalloc.c;

clean:
	rm *.o *.so *.out