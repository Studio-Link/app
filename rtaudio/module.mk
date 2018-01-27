#
# module.mk
#
# Copyright (C) 2018 Studio Link
#

MOD		:= rtaudio
$(MOD)_SRCS	+= rtaudio.c
$(MOD)_LFLAGS   += -lrtaudio -lstdc++

include mk/mod.mk
