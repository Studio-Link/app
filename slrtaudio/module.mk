#
# module.mk
#
# Copyright (C) 2018 Studio Link
#

MOD		:= slrtaudio
$(MOD)_SRCS	+= slrtaudio.c
$(MOD)_LFLAGS   += -lrtaudio -lstdc++

include mk/mod.mk
