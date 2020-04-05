#
# module.mk
#
# Copyright (C) 2020 Studio Link
#

MOD		:= slaudio
$(MOD)_SRCS	+= slaudio.c record.c
$(MOD)_LFLAGS   += -lsoundio -lsamplerate
ifeq ($(OS),linux)
	$(MOD)_LFLAGS   += -lpulse-simple -lpulse
endif
$(MOD)_CFLAGS   += -Wno-aggregate-return

include mk/mod.mk
