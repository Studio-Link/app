#
# module.mk
#
# Copyright (C) 2016 Studio Link
#

MOD		:= effectlive
$(MOD)_SRCS	+= effectlive.c
$(MOD)_LFLAGS	+= -lm

include mk/mod.mk
