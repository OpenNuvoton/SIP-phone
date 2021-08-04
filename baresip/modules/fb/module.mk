#
# module.mk - DirectFB video display module
#
# Copyright (C) 2010 Alfred E. Heggestad
# Copyright (C) 2013 Andreas Shimokawa <andi@fischlustig.de>.
#

MOD                := fb
$(MOD)_SRCS        += fb.c

include mk/mod.mk
