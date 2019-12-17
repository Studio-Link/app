#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= g722
$(MOD)_SRCS	+= g722.c g722.h g722_decode.c g722_encode.c

include mk/mod.mk
