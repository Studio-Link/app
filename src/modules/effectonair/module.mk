#
# module.mk
#
# Copyright (C) 2016 Studio Link
#

MOD		:= effectonair
$(MOD)_SRCS	+= effectonair.c
$(MOD)_LFLAGS	+= -lm

include mk/mod.mk
