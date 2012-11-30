
CFLAGS=-Wall -Werror

all: zaplib.so

zaplib.so: azaplib.o czaplib.o szaplib.o tzaplib.o util.o
	gcc -shared -o zaplib.so azaplib.o czaplib.o szaplib.o tzaplib.o util.o

azaplib.o: azaplib.c
	gcc -c -fpic $(CFLAGS) -o azaplib.o azaplib.c

czaplib.o: czaplib.c
	gcc -c -fpic $(CFLAGS) -o czaplib.o czaplib.c

szaplib.o: szaplib.c
	gcc -c -fpic $(CFLAGS) -o szaplib.o szaplib.c

tzaplib.o: tzaplib.c
	gcc -c -fpic $(CFLAGS) -o tzaplib.o tzaplib.c

util.o: util.c
	gcc -c -fpic $(CFLAGS) -o util.o util.c

clean:
	rm -fr *.so *.o tools/test tools/test.o

test: tools/test.c azaplib.o util.o
	gcc -o tools/test tools/test.c azaplib.o util.o

