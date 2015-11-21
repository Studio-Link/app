#
# module.mk
#
# Copyright (C) 2015 studio-link.de
#

MOD		:= webapp
$(MOD)_SRCS	+= webapp.c account.c contact.c chat.c
$(MOD)_SRCS	+= ws_baresip.c ws_contacts.c ws_chat.c ws_meter.c
$(MOD)_SRCS	+= websocket.c utils.c
$(MOD)_LFLAGS   += -lm -ljack

include mk/mod.mk
