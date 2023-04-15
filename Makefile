.PHONY:benchmark

all: lib benchmark lib

lib: libwlmalloc.a libwlmalloc.so

libwlmalloc.a: wlmalloc.o
	cd src; ar -crv libwlmalloc.a wlmalloc.o

wlmalloc.o:
	cd src; gcc -lpthread -c wlmalloc.c

libwlmalloc.so:
	cd src; gcc -lpthread -fpic -shared -o libwlmalloc.so wlmalloc.c

benchmark:
	cd benchmark; gcc -DGC ../src/wlgc.c ../src/log.c -o gcbench.out gcbench.c

# 有些环境的系统ls会出现问题，在此提供一个可单独编译的ls
ls: wlmalloc.o
	cd src; gcc -lpthread wlmalloc.o ls.c -o ls.out

clean:
	rm -f src/*.so src/*.o src/*.out src/*.a benchmark/*.out