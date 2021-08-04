/**
 * @file aec3.c For webrtc aec3 module interface
 *
 * Copyright (C) 2021 CHChen59
 */
#ifndef __AEC3_H__
#define __AEC3_H__

#include "modules/audio_processing/include/audio_processing.h"

#define MAX_CHANNELS         2

using namespace webrtc;

struct aec3 {
	uint32_t srate;
	uint32_t chan;
	uint32_t subframe_len;
	StreamConfig sStreamConfig;
	std::unique_ptr<webrtc::EchoControl> echo_controler;
};

int webrtc_aec3_alloc(struct aec3 **stp, void **ctx, struct aufilt_prm *prm);

int webrtc_aec3_encode_update(struct aufilt_enc_st **stp, void **ctx,
			     const struct aufilt *af, struct aufilt_prm *prm,
			     const struct audio *au);

int webrtc_aec3_encode(struct aufilt_enc_st *st, struct auframe *af);

int webrtc_aec3_decode_update(struct aufilt_dec_st **stp, void **ctx,
			     const struct aufilt *af, struct aufilt_prm *prm,
			     const struct audio *au);

int webrtc_aec3_decode(struct aufilt_dec_st *st, struct auframe *af);

#endif
