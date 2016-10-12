#
# module.mk
#
# Copyright (C) 2016 Studio Link
#

MOD		:= applive
$(MOD)_SRCS	+= applive.c account.c utils.c
$(MOD)_LFLAGS	+= -lm

include mk/mod.mk
