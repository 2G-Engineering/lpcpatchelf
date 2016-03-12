all: lpcpatchelf

CC = gcc

CFLAGS += -Wall -Wextra -std=c99 -Os

lpcpatchelf: lpcpatchelf.c 
	$(CC) $(CDEBUG) $(CFLAGS) lpcpatchelf.c -o lpcpatchelf -lelf

clean:
	$(RM) lpcpatchelf.o lpcpatchelf
