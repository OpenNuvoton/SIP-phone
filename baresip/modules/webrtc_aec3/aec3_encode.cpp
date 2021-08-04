/**
 * @file aec3_encode.c For webrtc aec3 module
 *
 * Copyright (C) 2021 CHChen59
 */

#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>

#include "aec3.h"
#include "modules/audio_processing/audio_buffer.h"


struct aec3_enc {
	struct aufilt_enc_st af;  /* inheritance */

	struct aec3 *aec3;
	uint8_t *tmp_buf;
	uint32_t tmp_buf_size;
	uint32_t tmp_buf_in;	
	uint32_t tmp_buf_out;	

	std::unique_ptr<AudioBuffer> CaptureAudioBuf;
};

static void enc_destructor(void *arg)
{
	struct aec3_enc *st = (struct aec3_enc *)arg;

	if(st->tmp_buf)
		mem_deref(st->tmp_buf);

	list_unlink(&st->af.le);
	mem_deref(st->aec3);
	st->aec3 = NULL;
}

int webrtc_aec3_encode_update(struct aufilt_enc_st **stp, void **ctx,
			     const struct aufilt *af, struct aufilt_prm *prm,
			     const struct audio *au)
{
	struct aec3_enc *st;
	int err;

	if (!stp || !af || !prm)
		return EINVAL;

	switch (prm->fmt) {

	case AUFMT_S16LE:
		break;

	default:
		warning("webrtc_aec: enc: unsupported sample format (%s)\n",
			aufmt_name((enum aufmt)prm->fmt));
		return ENOTSUP;
	}

	if (*stp)
		return 0;

	st = (struct aec3_enc *)mem_zalloc(sizeof(*st), enc_destructor);
	if (!st)
		return ENOMEM;

	err = webrtc_aec3_alloc(&st->aec3, ctx, prm);
	if (err)
		goto out;

	st->tmp_buf_size = st->aec3->subframe_len * st->aec3->chan * sizeof(uint16_t) * 2;
	st->tmp_buf = (uint8_t *)mem_alloc(st->tmp_buf_size, NULL);
	st->tmp_buf_in = 0;
	st->tmp_buf_out = 0;
	
	if(st->tmp_buf == NULL)
	{
		err = ENOMEM;
		goto out;
	}

	st->CaptureAudioBuf = std::make_unique<AudioBuffer>(
		st->aec3->srate, st->aec3->chan,
		st->aec3->srate, st->aec3->chan,
		st->aec3->srate, st->aec3->chan);

 out:
	if (err)
		mem_deref(st);
	else
		*stp = (struct aufilt_enc_st *)st;

	return err;
}

static int encode(
	struct aec3_enc *enc, 
	uint16_t *sampv, 
	size_t sampc
)
{
	struct aec3 *aec3 = enc->aec3;
	int ret = 0;
	uint32_t frame_len = aec3->subframe_len * aec3->chan * sizeof(uint16_t);
	int i;
	uint8_t *enc_buf = (uint8_t *)sampv;
	uint32_t enc_len = 0;

	//sampc=ptime * sample rate * channel, aec3 subframe_len = 10ms * sample rate
	for (i = 0; i < sampc; i += (aec3->subframe_len * aec3->chan)) {
		if((enc->tmp_buf_out + frame_len) >= enc->tmp_buf_in)
			break;

		enc->CaptureAudioBuf->CopyFrom((const int16_t*)(enc->tmp_buf + enc->tmp_buf_out), aec3->sStreamConfig);
		enc->tmp_buf_out += frame_len; 

		if(aec3->echo_controler){
			aec3->echo_controler->AnalyzeCapture(enc->CaptureAudioBuf.get());
			aec3->echo_controler->ProcessCapture(enc->CaptureAudioBuf.get(), false);
		}

		enc->CaptureAudioBuf->CopyTo(aec3->sStreamConfig, (int16_t*)(enc_buf + enc_len));
		enc_len += frame_len;				
	}

	return 0;
}


int webrtc_aec3_encode(struct aufilt_enc_st *st, struct auframe *af)
{
	struct aec3_enc *enc = (struct aec3_enc *)st;
	int err = 0;
	uint32_t frame_bytes;

	if (!st || !af)
		return EINVAL;

	frame_bytes = af->sampc * sizeof(uint16_t);

	if((frame_bytes + enc->tmp_buf_in) > enc->tmp_buf_size)
	{
		uint8_t *tmp;
		tmp = (uint8_t *)mem_realloc(enc->tmp_buf, frame_bytes + enc->tmp_buf_in + 100);
		
		if(tmp == NULL)
			return ENOMEM;

		enc->tmp_buf = tmp;
		enc->tmp_buf_size = frame_bytes + enc->tmp_buf_in;
		info("Resize webrtc_aec3_encode audio buffer %d bytes \n", enc->tmp_buf_size);
	}

	memcpy(enc->tmp_buf + enc->tmp_buf_in, af->sampv, frame_bytes);
	enc->tmp_buf_in += frame_bytes;

	encode(enc, (uint16_t *)af->sampv, af->sampc);

	if(enc->tmp_buf_out) {
		int32_t move_size = enc->tmp_buf_in - enc->tmp_buf_out;
		
		if(move_size > 0) {
			memmove(enc->tmp_buf, enc->tmp_buf + enc->tmp_buf_out, move_size); 
			enc->tmp_buf_out = 0;
			enc->tmp_buf_in -= move_size;	
		}
		else {
			enc->tmp_buf_out = 0;
			enc->tmp_buf_in = 0;			
		}
	}

	return 0;
}
