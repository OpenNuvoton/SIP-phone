#
# module.mk
#
# Copyright (C) 2010 Alfred E. Heggestad
#


WEBRTC3_SDKPATH	:= ../WebRTC_AEC
WEBRTC3_LIBPATH	:= $(DESTDIR)/usr/lib


MOD		:= webrtc_aec3

$(MOD)_SRCS	+= aec3.cpp
$(MOD)_SRCS	+= aec3_encode.cpp
$(MOD)_SRCS	+= aec3_decode.cpp

CPPFLAGS	+= -I $(WEBRTC3_SDKPATH)/src -I $(WEBRTC3_SDKPATH)/src/third_party/abseil-cpp

$(MOD)_LFLAGS	+= \
	-L$(WEBRTC3_LIBPATH) \
	-lwebrtc_aec \
	-lstdc++


include mk/mod.mk
