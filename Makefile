PWD=$(shell pwd)

VERSION1=1
VERSION2=0
VERSION3=0

VERSION=$(VERSION1).$(VERSION2).$(VERSION3)
VERSION_TRANSITIONAL=$(VERSION1).$(VERSION2)
VERSION_ANCHOR=$(VERSION1)
INSTALL_PATH=/usr/local/lib

SRC_PATH=$(PWD)
OUTPUT_PATH=$(PWD)/build

ZAPLIB_SO_FILENAME=libzaplib.so
ZAPLIB_SO_NAME=$(ZAPLIB_SO_FILENAME).$(VERSION_ANCHOR)
ZAPLIB_SO_INSTALLPATH=$(INSTALL_PATH)/$(ZAPLIB_SO_NAME)

CC=gcc
CFLAGS=-g -Wall -Werror 

all: $(OUTPUT_PATH)/zaplib.so

$(OUTPUT_PATH)/zaplib.so: $(OUTPUT_PATH)/azaplib.o $(OUTPUT_PATH)/czaplib.o \
           $(OUTPUT_PATH)/szaplib.o $(OUTPUT_PATH)/lnb.o \
           $(OUTPUT_PATH)/tzaplib.o $(OUTPUT_PATH)/util.o
	$(CC) -shared $(CFLAGS) -Wl,-soname,libzaplib.so.1 \
	      -o $(OUTPUT_PATH)/libzaplib.so.1 \
	      $(OUTPUT_PATH)/azaplib.o $(OUTPUT_PATH)/czaplib.o \
	      $(OUTPUT_PATH)/szaplib.o $(OUTPUT_PATH)/lnb.o $(OUTPUT_PATH)/tzaplib.o \
	      $(OUTPUT_PATH)/util.o

$(OUTPUT_PATH)/azaplib.o: $(SRC_PATH)/azaplib.c
	$(CC) -c -fpic $(CFLAGS) -o $(OUTPUT_PATH)/azaplib.o $(SRC_PATH)/azaplib.c

$(OUTPUT_PATH)/czaplib.o: $(SRC_PATH)/czaplib.c
	$(CC) -c -fpic $(CFLAGS) -o $(OUTPUT_PATH)/czaplib.o $(SRC_PATH)/czaplib.c

$(OUTPUT_PATH)/szaplib.o: $(SRC_PATH)/szaplib.c
	$(CC) -c -fpic $(CFLAGS) -o $(OUTPUT_PATH)/szaplib.o $(SRC_PATH)/szaplib.c

$(OUTPUT_PATH)/lnb.o: $(SRC_PATH)/lnb.c
	$(CC) -c -fpic $(CFLAGS) -o $(OUTPUT_PATH)/lnb.o $(SRC_PATH)/lnb.c

$(OUTPUT_PATH)/tzaplib.o: $(SRC_PATH)/tzaplib.c
	$(CC) -c -fpic $(CFLAGS) -o $(OUTPUT_PATH)/tzaplib.o $(SRC_PATH)/tzaplib.c

$(OUTPUT_PATH)/util.o: $(SRC_PATH)/util.c
	$(CC) -c -fpic $(CFLAGS) -o $(OUTPUT_PATH)/util.o $(SRC_PATH)/util.c

clean:
	rm -fr $(OUTPUT_PATH)/* $(INSTALL_PATH)/$(ZAPLIB_SO_FILENAME)*

install:
	cp $(OUTPUT_PATH)/$(ZAPLIB_SO_NAME) $(INSTALL_PATH)
	ln -s $(INSTALL_PATH)/$(ZAPLIB_SO_NAME) $(INSTALL_PATH)/$(ZAPLIB_SO_FILENAME)
	ln -s $(INSTALL_PATH)/$(ZAPLIB_SO_NAME) $(INSTALL_PATH)/$(ZAPLIB_SO_FILENAME).$(VERSION)

test: tools/test.c azaplib.o util.o
	$(CC) -o tools/test tools/test.c azaplib.o util.o

