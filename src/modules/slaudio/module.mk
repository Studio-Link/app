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
$(MOD)_CFLAGS   += -DSOUNDIO_STATIC_LIBRARY=1 -Wno-cast-align

include mk/mod.mk
