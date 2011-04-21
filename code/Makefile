##################################################
## General configuration
## =====================

# Every Makefile should contain this line:
SHELL = /bin/sh

# Program for compiling C programs. 
CC = gcc

# Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers). 
CFLAGS = -Wall -Wextra -g -ansi -D_POSIX_SOURCE

##################################################
## Setup files variables
## =====================

# Source files to compile and link together
srcs = digenv.c
objs = $(srcs:.c=.o)

##################################################
## Ordinary targets
## ================

# http://www.gnu.org/software/make/manual/make.html#Phony-Targets
# These are not the name of files that will be created by their recipes.
.PHONY: all clean run

digenv: $(objs)
	$(CC) $(LDFLAGS) $^ -o $@

run: digenv
	./$<

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	-rm -f *.o digenv

