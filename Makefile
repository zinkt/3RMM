.PHONY:test_benchmark

all: lib test_benchmark lib

lib: libwlmalloc.a libwlmalloc.so

libwlmalloc.a: wlmalloc.o
	cd src; ar -crv libwlmalloc.a wlmalloc.o

wlmalloc.o:
	cd src; gcc -lpthread -c wlmalloc.c

libwlmalloc.so:
	cd src; gcc -lpthread -fpic -shared -o libwlmalloc.so wlmalloc.c

test_benchmark:
	cd test_benchmark; gcc -DGC ../src/wlgc.c ../src/log.c -o gcbench.out gcbench.c
	cd test_benchmark; gcc -lpthread ../src/wlmalloc.c tri_mod_test.c -o tri_mod_test.out

# 有些环境的系统ls会出现问题，在此提供一个可单独编译的ls
ls:
	cd src; gcc ls.c -o ls

clean:
	rm -f src/*.so src/*.o src/*.out src/*.a src/ls test_benchmark/*.out