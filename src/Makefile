
test-bench:wlmalloc.c test-bench.c
	gcc -g -o test-bench.out wlmalloc.c test-bench.c -lpthread

test:wlmalloc.c test.c
	gcc -g -o test.out wlmalloc.c test.c -lpthread

so:
	gcc -c -fPIC -o wlmalloc.o wlmalloc.c;
	gcc -shared -o libwlmalloc.so wlmalloc.o;

clean:
	rm *.o *.so *.out