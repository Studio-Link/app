#
# module.mk
#
# Copyright (C) 2018 Studio Link
#

MOD		:= slrtaudio
$(MOD)_SRCS	+= slrtaudio.c record.c
$(MOD)_LFLAGS   += -lrtaudio -lstdc++ -lsamplerate
ifeq ($(OS),linux)
	$(MOD)_LFLAGS   += -lpulse-simple -lpulse
endif
$(MOD)_CFLAGS   += -Wno-aggregate-return

include mk/mod.mk
