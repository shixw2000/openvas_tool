program=util_openvas
libutil=libutil_openvas.so
libphp=libphp_openvas.so

root_dir=/usr/local/openvas/depends

CC=gcc
BIN_FLAG=-Wall -g
BIN_LINK=
SO_FLAG=-Wall -g -fPIC
SO_LINK=-shared
INC=-I$(root_dir)/include/hiredis -I$(root_dir)/include/libxml2 
LIBS=-Wl,-rpath=$(root_dir)/lib -L$(root_dir)/lib -lhiredis -lxml2 -lrt
UTIL_LIBS=$(libutil)


SHARE_SRC=base_openvas.c task_openvas.c comm_misc.c
SHARE_OBJS=$(SHARE_SRC:%.c=%.o)

PHP_SRC=php_openvas.c 
PHP_OBJS=$(PHP_SRC:%.c=%.o)

UTIL_SRC=openvas_business.c llist.c openvas_opts.c\
 hydra_business.c 
UTIL_OBJS=$(UTIL_SRC:%.c=%.o)

PROG_SRC=main.c test.c
PROG_OBJS=$(PROG_SRC:%.c=%.o)

default:$(program) $(libphp)
.PHONY:default

lib:$(libutil)
.PHONY:lib

php:$(libphp)
.PHONY:php 

$(libutil):$(UTIL_OBJS) $(SHARE_OBJS) $(PHP_OBJS)
	$(CC) $(SO_LINK) $^ -o $@ $(LIBS)

$(libphp):$(PHP_OBJS) $(SHARE_OBJS)
	$(CC) $(SO_LINK) $^ -o $@ $(LIBS)

$(program):$(PROG_OBJS) $(libutil)
	$(CC) $(BIN_LINK) $(PROG_OBJS) -o $@ $(UTIL_LIBS) $(LIBS)

$(PHP_OBJS):%.o:%.c
	$(CC) $(SO_FLAG) $(INC) -c $< -o $@

$(SHARE_OBJS):%.o:%.c
	$(CC) $(SO_FLAG) $(INC) -c $< -o $@

$(UTIL_OBJS):%.o:%.c
	$(CC) $(SO_FLAG) $(INC) -c $< -o $@

$(PROG_OBJS):%.o:%.c
	$(CC) $(BIN_FLAG) $(INC) -c $< -o $@

clean:
	@rm -f $(program) $(libphp) $(libutil) $(UTIL_OBJS) \
	$(SHARE_OBJS) $(PHP_OBJS) $(PROG_OBJS)
.PHONY:clean

