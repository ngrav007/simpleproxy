src = $(wildcard *.c)
obj = $(src:.c=.o)
CC = gcc
LDFLAGS = -lnsl

%.o:%.c
	$(CC) -c $^ $(LDFLAGS)

# a.out: $(obj) CacheTable.h checkerr.h
# 	$(CC) -g -o $@ $^ $(LDFLAGS)

a.out: $(obj) CacheTable.h checkerr.h
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean

clean:
	rm -f $(obj) a.out core.* vgcore.*
