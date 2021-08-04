/**
 * @file aec3_decode.c For webrtc aec3 module
 *
 * Copyright (C) 2021 CHChen59
 */

#include <string.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>

#include "aec3.h"

#include "modules/audio_processing/audio_buffer.h"

struct aec3_dec {
	struct aufilt_dec_st af;  /* inheritance */
	struct aec3 *aec3;
	uint8_t *tmp_buf;
	uint32_t tmp_buf_size;
	uint32_t tmp_buf_in;	
	uint32_t tmp_buf_out;	

	std::unique_ptr<AudioBuffer> RenderAudioBuf;
};


static void dec_destructor(void *arg)
{
	struct aec3_dec *st = (struct aec3_dec *)arg;
	
	if(st->tmp_buf)
		mem_deref(st->tmp_buf);

	list_unlink(&st->af.le);
	mem_deref(st->aec3);
}

int webrtc_aec3_decode_update(struct aufilt_dec_st **stp, void **ctx,
			     const struct aufilt *af, struct aufilt_prm *prm,
			     const struct audio *au)
{
	struct aec3_dec *st;
	int err;

	if (!stp || !af || !prm)
		return EINVAL;

	switch (prm->fmt) {

	case AUFMT_S16LE:
		break;

	default:
		warning("webrtc_aec: dec: unsupported sample format (%s)\n",
			aufmt_name((enum aufmt)prm->fmt));
		return ENOTSUP;
	}


	if (*stp)
		return 0;

	st = (struct aec3_dec *)mem_zalloc(sizeof(*st), dec_destructor);
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

	st->RenderAudioBuf = std::make_unique<AudioBuffer>(
		st->aec3->srate, st->aec3->chan,
		st->aec3->srate, st->aec3->chan,
		st->aec3->srate, st->aec3->chan);

 out:
	if (err)
	{
		if(st->tmp_buf)
			mem_deref(st->tmp_buf);
		mem_deref(st);
	}
	else
	{
		*stp = (struct aufilt_dec_st *)st;
	}

	return err;
}

static int decode(
	struct aec3_dec *dec
)
{
	struct aec3 *aec3 = dec->aec3;
	int ret = 0;
	uint32_t frame_len = aec3->subframe_len * aec3->chan * sizeof(uint16_t);
	int i;
	uint32_t total_frames = (dec->tmp_buf_in - dec->tmp_buf_out) / aec3->chan / sizeof(uint16_t);
	
	for (i = 0; i < total_frames; i += aec3->subframe_len) {

		dec->RenderAudioBuf->CopyFrom((const int16_t*)dec->tmp_buf + dec->tmp_buf_out, aec3->sStreamConfig);
		dec->tmp_buf_out += frame_len; 

		if(aec3->echo_controler){
			aec3->echo_controler->AnalyzeRender(dec->RenderAudioBuf.get());
		}
	}

	return ret;
}

int webrtc_aec3_decode(struct aufilt_dec_st *st, struct auframe *af)
{
	struct aec3_dec *dec = (struct aec3_dec *)st;
	int err = 0;
	uint32_t frame_bytes;

	if (!st || !af)
		return EINVAL;

	frame_bytes = af->sampc * sizeof(uint16_t);
	
	if((frame_bytes + dec->tmp_buf_in) > dec->tmp_buf_size)
	{
		uint8_t *tmp;
		tmp = (uint8_t *)mem_realloc(dec->tmp_buf, frame_bytes + dec->tmp_buf_in + 100);
		
		if(tmp == NULL)
			return ENOMEM;

		dec->tmp_buf = tmp;
		dec->tmp_buf_size = frame_bytes + dec->tmp_buf_in;
		info("Resize wertc_aec3_decode audio buffer %d bytes\n", dec->tmp_buf_size);
	}

	memcpy(dec->tmp_buf + dec->tmp_buf_in, af->sampv, frame_bytes);
	dec->tmp_buf_in += frame_bytes;

	// write to echo controller render buffer
	decode(dec);

	if(dec->tmp_buf_out) {
		uint32_t move_size = dec->tmp_buf_in - dec->tmp_buf_out;
		
		if(move_size) {
			memmove(dec->tmp_buf, dec->tmp_buf + dec->tmp_buf_out, move_size); 
			dec->tmp_buf_out = 0;
			dec->tmp_buf_in -= move_size;	
		}
		else {
			dec->tmp_buf_out = 0;
			dec->tmp_buf_in = 0;			
		}
	}

	return 0;
}

