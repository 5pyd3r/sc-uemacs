CC=gcc
arch=$(shell echo "(printf (symbol->string (machine-type)))" | scheme -q)
CFLAGS=-I/usr/lib/csv9.4.1/$(arch)/ 
LDFLAGS=-lm -lpthread -ldl -lcurses

src=uemacs.ss
Scheme = scheme -q

obj = ${src:%.ss=%.so}

.SUFFIXES :
.SUFFIXES : .ss .so .c .o

all : uemacs uemacs.boot

uemacs.boot : uemacs.so
	echo '(reset-handler abort)'\
			'(make-boot-file "$@" (quote ("scheme" "petite")) "uemacs.so")' \
             | ${Scheme}

uemacs : main.o uemacs.o
	$(CC) -o uemacs $^ ../chezscheme/$(arch)/boot/$(arch)/kernel.o $(LDFLAGS)

.ss.so:
	echo '(compile-file "$^")' | $(Scheme)

.c.o:
	$(CC) $(CFLAGS) -c $^

install : uemacs.boot uemacs
	install -m 555 uemacs /usr/bin/uemacs
	install -m 444 uemacs.boot /usr/lib/csv9.4.1/$(arch)/uemacs.boot

clean :
	rm -f *.o
	rm -f *.so
	rm uemacs.boot
	rm uemacs
