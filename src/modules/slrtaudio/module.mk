#
# module.mk
#
# Copyright (C) 2018 Studio Link
#

MOD		:= slrtaudio
$(MOD)_SRCS	+= slrtaudio.c record.c
ifeq ($(OS),linux)
	$(MOD)_LFLAGS   += -lrtaudio -lstdc++ -lpulse-simple -lpulse
else
	$(MOD)_LFLAGS   += -lrtaudio -lstdc++
endif
$(MOD)_CFLAGS   += -Wno-aggregate-return

include mk/mod.mk
