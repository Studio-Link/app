#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= effect
$(MOD)_SRCS	+= effect.c
$(MOD)_LFLAGS	+= -lm

include mk/mod.mk
