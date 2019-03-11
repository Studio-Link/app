#
# module.mk
#
# Copyright (C) 2015 Studio-Link.de
#

MOD		:= gpio
$(MOD)_SRCS	+= event_gpio.c
$(MOD)_SRCS	+= gpio.c
$(MOD)_LFLAGS	+= -lpthread

include mk/mod.mk
