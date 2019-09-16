#
# module.mk
#
# Copyright (C) 2018 Studio Link
#

MOD		:= slrtaudio
$(MOD)_SRCS	+= slrtaudio.c record.c
ifeq ($(OS),linux)
	$(MOD)_LFLAGS   += -lrtaudio -lstdc++ -lsamplerate -lpulse-simple -lpulse
else
	$(MOD)_LFLAGS   += -lrtaudio -lstdc++ -lsamplerate
endif
$(MOD)_CFLAGS   += -Wno-aggregate-return

include mk/mod.mk
