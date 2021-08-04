#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

SPEEX_SDKPATH	:= ../speex-1.2.0
SPEEX_LIBPATH	:= $(DESTDIR)/usr/lib

MOD		:= speex
$(MOD)_SRCS	+= speex.c
$(MOD)_LFLAGS	+= -L$(SPEEX_LIBPATH) -lspeex
$(MOD)_CFLAGS	+= -I$(SPEEX_SDKPATH)/include -Wno-strict-prototypes

include mk/mod.mk
