CC=gcc
LIBMX_CMD_AGENT_PATH=../bin
LIBYUAREL_PATH=libyuarel
LIBPARSON_PATH=.
INCLUDE_PATH=-I../include -I../include/libmx-cmd-agent -I../src/http_server/nng/1.1.1/include/nng
#CC:=arm-linux-gnueabihf-gcc
#CFLAGS=-L../bin -I../include -pthread -lmx-cmd-agent -lparson -Wall
#CFLAGS=-L../bin -I../include -pthread -lmx-cmd-agent  -Wall
#CFLAGS= -I../include -I../include/libmx-cmd-agent -I ../src/http_server/nng/1.1.1/include/nng/ -pthread -lmx-cmd-agent -lparson -lyuarel -L. -L$(LIBYUAREL_PATH) -L$(LIBMX_CMD_AGENT_PATH)
CFLAGS= $(INCLUDE_PATH) -pthread -lmx-cmd-agent -lparson -lyuarel -L$(LIBPARSON_PATH) -L$(LIBYUAREL_PATH) -L$(LIBMX_CMD_AGENT_PATH)
  
all :	
#	cp libyuarel/libyuarel.so.1 libyuarel.so
	$(CC) -o portd_rest portd_rest.c $(CFLAGS)
		
.PHONY: clean install
clean:
	rm -f portd_rest

install:
	cp ../bin/libmx-cmd-agent.so . 
