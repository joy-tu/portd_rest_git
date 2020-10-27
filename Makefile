#CROSS_COMPILER:=arm-linux-gnueabihf-
#CC:=$(CROSS_COMPILER)gcc
CC:=gcc
STRIP:=$(CROSS_COMPILER)strip
LDFLAG:=
EXT_INCLUDES:=

HTTP_SERVER_INC = src/http_server/nng/1.1.1/include

#for handling the REST related service
HTTP_SERVER_SRC=src/http_server/http_server_nng.c
DATA_LOGGER_SRC=src/data_logger/data_logger.c
REST_SRC=src/rest_parser/rest_parser.c $(DATA_LOGGER_SRC) $(HTTP_SERVER_SRC) 

SRCS=$(REST_SRC)

OUT_DIR=bin

OBJS = $(patsubst %.c, %.o, $(SRCS))

OUT_OBJS=$(addprefix $(OUT_DIR)/, $(OBJS))

TARGET=$(OUT_DIR)/libmx-cmd-agent.so

CFLAGS=-Iinclude -Iinclude/libmx-cmd-agent -I$(HTTP_SERVER_INC) $(EXT_INCLUDES) -pthread -L./$(OUT_DIR) -Wall 

.PHONY: clean

all : $(OUT_OBJS)
	sh buildnng.sh $(CC)
	$(CC) -shared -fPIC $(OUT_OBJS) -lnng -pthread -L./$(OUT_DIR) $(LDFLAG) -o $(TARGET)
	$(STRIP) $(TARGET)

$(OUT_DIR)/%.o:%.c
	mkdir -p $(dir $@)
	$(CC) -fPIC $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUT_DIR)
