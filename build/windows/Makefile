#
# Makefile
#
# Copyright (C) 2010 - 2016 Creytiv.com
#

ifeq ($(BUILD_OS),windows64)
	TUPLE   := x86_64-w64-mingw32
	SSLARCH	:= mingw64
endif
ifeq ($(BUILD_OS),windows32)
	TUPLE	:= i686-w64-mingw32
	SSLARCH	:= mingw	
endif


# Tools
SYSROOT   := /local/$(TUPLE)
CC        := $(TUPLE)-gcc
CXX       := $(TUPLE)-g++
RANLIB    := $(TUPLE)-ranlib
AR        := $(TUPLE)-ar
PWD       := $(shell pwd)


# Compiler and Linker Flags
#

CFLAGS    := -DFD_SETSIZE=4096 \
	-isystem $(PWD)/openssl/include $(PWD)/opus/include -g -gdwarf-2

LFLAGS    := -L$(SYSROOT)/lib/ \
	-L$(PWD)/openssl \
	-L$(PWD)/rem \
	-L$(PWD)/opus

# workaround for linker order (note, the order is important)
LIBS	:= -lrem -lssl -lcrypto -lwsock32 -lws2_32 -liphlpapi -lwinmm -lcrypt32 -lopus


COMMON_FLAGS := CC=$(CC) \
		CXX=$(CXX) \
		RANLIB=$(RANLIB) \
		EXTRA_CFLAGS="$(CFLAGS)" \
		EXTRA_LFLAGS="$(LFLAGS)" \
		LIBS="$(LIBS)" \
		SYSROOT=$(SYSROOT) \
		SYSROOT_ALT= \
		HAVE_GETOPT=1 \
		HAVE_LIBRESOLV= \
		HAVE_PTHREAD= \
		HAVE_PTHREAD_RWLOCK= \
		HAVE_LIBPTHREAD= \
		HAVE_INET_PTON=1 \
		HAVE_INET6=1 \
		PEDANTIC= \
		OPT_SIZE=1 \
		OS=win32 \
		USE_OPENSSL=yes \
		USE_OPENSSL_DTLS=yes \
		USE_OPENSSL_SRTP=yes \
		USE_ZLIB=

#default:	retest baresip
default:	baresip

libre.a: Makefile
	@rm -f re/libre.*
	make $@ -C re $(COMMON_FLAGS)

librem.a:	Makefile libre.a
	@rm -f rem/librem.*
	@make $@ -C rem $(COMMON_FLAGS)

.PHONY: retest
test: retest
retest:		Makefile librem.a libre.a
	@rm -f retest/retest
	make -C retest $(COMMON_FLAGS) LIBRE_SO=$(PWD)/re \
		LIBREM_PATH=$(PWD)/rem
	cd retest && $(WINE) retest -r

.PHONY: baresip
baresip:	Makefile librem.a libre.a
	@rm -f baresip/baresip.exe baresip/src/static.c
	PKG_CONFIG_LIBDIR="$(SYSROOT)/lib/pkgconfig" \
	make selftest.exe baresip.exe -C baresip $(COMMON_FLAGS) STATIC=1 \
		LIBRE_SO=$(PWD)/re LIBREM_PATH=$(PWD)/rem EXTRA_MODULES="opus webapp"

.PHONY: openssl
openssl:
	cd openssl && \
		CC=$(CC) RANLIB=$(RANLIB) AR=$(AR) \
		./Configure $(SSLARCH) no-shared && \
		make build_libs

clean:
	make distclean -C baresip
	make distclean -C retest
	make distclean -C rem
	make distclean -C re

info:
	make $@ -C re $(COMMON_FLAGS)

dump:
	@echo "TUPLE = $(TUPLE)"
	@echo "WINE  = $(WINE)"
