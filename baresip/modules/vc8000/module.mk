#
# module.mk
#
# Copyright (C) 2021 CHChen
#

MOD		:= vc8000
$(MOD)_SRCS	+= vc8000.c
$(MOD)_SRCS	+= vc8000_h264d.c
$(MOD)_SRCS	+= vc8000_v4l2.c

include mk/mod.mk
