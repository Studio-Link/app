#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= auice
$(MOD)_SRCS	+= auice.c
$(MOD)_LFLAGS	+= -lshout -lmp3lame

include mk/mod.mk
