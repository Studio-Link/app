#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= auicecast
$(MOD)_SRCS	+= auicecast.c
$(MOD)_LFLAGS	+= -lshout -lmp3lame

include mk/mod.mk
