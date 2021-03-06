ARCH := $(shell uname -m)

CFLAGS := -O3 -s -fomit-frame-pointer
DEFINES := -DNDEBUG

ifeq ($(ARCH),x86_64)
CFLAGS += -msse4.1
endif

include build.mk
