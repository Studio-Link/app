#
# module.mk
#
# Copyright (C) 2013-2019 studio-link.de
#

MOD		:= webapp
$(MOD)_SRCS	+= webapp.c account.c contact.c vumeter.c option.c
$(MOD)_SRCS	+= chat.c
$(MOD)_SRCS	+= ws_baresip.c ws_contacts.c ws_meter.c ws_calls.c
$(MOD)_SRCS	+= ws_options.c ws_chat.c ws_rtaudio.c
$(MOD)_SRCS	+= websocket.c utils.c
$(MOD)_LFLAGS   += -lm -lFLAC

include mk/mod.mk
