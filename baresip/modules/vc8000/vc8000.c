/**
 * @file vc8000.c: vc8000 for ma35d1 hardware decoder
 *
 * Copyright (C) 2021 CHChen59
 */
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "vc8000.h"

static struct vc8000_vidcodec h264 = {
	.vc = {
		.name      = "H264",
		.variant   = "packetization-mode=0",
		.decupdh   = vc8000_decode_update,
		.dech      = vc8000_decode_h264,
		.fmtp_ench = vc8000_h264_fmtp_enc,
		.fmtp_cmph = vc8000_h264_fmtp_cmp,
	//	.packetizeh= avcodec_packetize,  for transmit h264 encoded frame
	},
};

static struct vc8000_vidcodec h264_1 = {
	.vc = {
		.name      = "H264",
		.variant   = "packetization-mode=1",
		.decupdh   = vc8000_decode_update,
		.dech      = vc8000_decode_h264,
		.fmtp_ench = vc8000_h264_fmtp_enc,
		.fmtp_cmph = vc8000_h264_fmtp_cmp,
	},
};


static int module_init(void)
{
	vidcodec_register(baresip_vidcodecl(), (struct vidcodec *)&h264);
	vidcodec_register(baresip_vidcodecl(), (struct vidcodec *)&h264_1);
	vc8000_init();
	return 0;
}

static int module_close(void)
{
	vidcodec_unregister((struct vidcodec *)&h264);
	vidcodec_unregister((struct vidcodec *)&h264_1);
	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(vc8000) = {
	"vc8000",
	"codec",
	module_init,
	module_close
};
