#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= jack
$(MOD)_SRCS	+= jack.c
$(MOD)_LFLAGS	+= -ljack -lm

include mk/mod.mk
