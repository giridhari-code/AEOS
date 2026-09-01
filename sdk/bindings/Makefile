# AEOS native kernel client library
#
# Builds libaeos_kernel.so: the C ABI consumed by sdk/py/aeos_sdk/ffi.py.

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -Werror -fPIC -std=c11
LDFLAGS ?= -shared

SRC = src/aeos_kernel.c
OUT = src/libaeos_kernel.so

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC)

clean:
	rm -f $(OUT)
