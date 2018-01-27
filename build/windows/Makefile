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
	-isystem $(PWD)/openssl/include -I$(PWD) -I $(PWD)/my_include -g -gdwarf-2

LFLAGS    := -L$(SYSROOT)/lib/ \
	-L$(PWD)/openssl \
	-L$(PWD)/rem \
	-L$(PWD)/opus \
	-L$(PWD)/my_include

# workaround for linker order (note, the order is important)
LIBS	:= -lrem -lssl -lcrypto -lwsock32 -lws2_32 -liphlpapi -lole32 -lwinmm -luuid -lksuser -lcrypt32 -lopus -lFLAC -lrtaudio -lstdc++


COMMON_FLAGS := CC=$(CC) \
		CXX=$(CXX) \
		RANLIB=$(RANLIB) \
		EXTRA_CFLAGS="$(CFLAGS)" \
		EXTRA_LFLAGS="$(LFLAGS) -static" \
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
		USE_ZLIB= \
		RELEASE=1

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
	make baresip.exe -C baresip $(COMMON_FLAGS) STATIC=1 \
		LIBRE_SO=$(PWD)/re LIBREM_PATH=$(PWD)/rem \
		MODULES="opus wincons ice g711 turn stun uuid auloop webapp winwave rtaudio"
	cp -a baresip/baresip.exe studio-link-standalone.exe
	make clean -C baresip $(COMMON_FLAGS)
	make libbaresip.a -C baresip $(COMMON_FLAGS) STATIC=1 \
		LIBRE_SO=$(PWD)/re LIBREM_PATH=$(PWD)/rem \
		MODULES="opus ice g711 turn stun uuid apponair effectonair" \
		EXTRA_CFLAGS="$(CFLAGS) -DSLIVE"
	cp -a baresip/libbaresip.a libbaresip_onair.a 
	make clean -C baresip $(COMMON_FLAGS)
	make libbaresip.a -C baresip $(COMMON_FLAGS) STATIC=1 \
		LIBRE_SO=$(PWD)/re LIBREM_PATH=$(PWD)/rem \
		MODULES="opus ice g711 turn stun uuid webapp effect" \
		EXTRA_CFLAGS="$(CFLAGS) -DSLPLUGIN"

.PHONY: openssl
openssl:
	cd openssl && \
		CC=$(CC) RANLIB=$(RANLIB) AR=$(AR) \
		./Configure $(SSLARCH) no-shared no-threads && \
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
