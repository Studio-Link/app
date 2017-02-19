#
# module.mk
#
# Copyright (C) 2016 studio-link.de
#

MOD		:= webapp
$(MOD)_SRCS	+= webapp.c account.c contact.c chat.c vumeter.c option.c
$(MOD)_SRCS	+= mono.c record.c routing.c
$(MOD)_SRCS	+= ws_baresip.c ws_contacts.c ws_chat.c ws_meter.c ws_calls.c
$(MOD)_SRCS	+= ws_options.c
$(MOD)_SRCS	+= websocket.c utils.c
$(MOD)_LFLAGS   += -lm -lFLAC

include mk/mod.mk
