/**
 * @file aec3.c For webrtc aec3 module interface
 *
 * Copyright (C) 2021 CHChen59
 */

#include <re.h>
#include <rem.h>
#include <baresip.h>
#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

#include "aec3.h"

#include "api/audio/echo_canceller3_factory.h"
#include "api/audio/echo_canceller3_config.h"

static void aec3_destructor(void *arg)
{
	struct aec3 *st = (struct aec3 *)arg;
}

int webrtc_aec3_alloc(struct aec3 **stp, void **ctx, struct aufilt_prm *prm)
{
	struct aec3 *aec3;
	int err = 0;
	int r;

	if (!stp || !ctx || !prm)
		return EINVAL;

	if (prm->ch > MAX_CHANNELS) {
		warning("webrtc_aec3: unsupported channels (%u > %u)\n",
			prm->ch, MAX_CHANNELS);
		return ENOTSUP;
	}

	if (*ctx) {
		//already allocate aec3, ready to enable echo controller
		aec3 = (struct aec3 *)*ctx;

		if (prm->srate != aec3->srate) {
			warning("webrtc_aec3: sample rate mismatch\n");
			return ENOTSUP;
		}

		if (prm->ch != aec3->chan) {
			warning("webrtc_aec3: channel mismatch\n");
			return ENOTSUP;
		}

		//enable echo controller
		EchoCanceller3Config aec_config;
		aec_config.filter.export_linear_aec_output = false;
		EchoCanceller3Factory aec_factory = EchoCanceller3Factory(aec_config);
		aec3->echo_controler = aec_factory.Create(aec3->srate, aec3->chan, aec3->chan);

		if(aec3->echo_controler == NULL)
		{
			warning("webrtc_aec3: unable create echo controllder\n");
			return ENOTSUP;
		}
		
		aec3->sStreamConfig = StreamConfig(aec3->srate, aec3->chan, false);
		aec3->subframe_len = aec3->sStreamConfig.num_frames(); 
		aec3->echo_controler->SetAudioBufferDelay(10);		//ptime

		*stp = (struct aec3 *)mem_ref(*ctx);
		return 0;
	}

	if(prm->srate < 16000)
	{
		warning("webrtc_aec3: sample rate not support \n");
		return ENOTSUP;
	}

	aec3 = (struct aec3 *)mem_zalloc(sizeof(*aec3), aec3_destructor);
	if (!aec3)
		return ENOMEM;

	aec3->srate = prm->srate;
	aec3->chan = prm->ch;

	info("webrtc_aec3: creating shared state:"
	     " [%u Hz, %u channels]\n",
	     prm->srate, prm->ch);

	*stp = aec3;
	*ctx = aec3;
	return 0;
}

static struct aufilt webrtc_aec3 = {
	.le      = LE_INIT,
	.name    = "webrtc_aec3",
	.encupdh = webrtc_aec3_encode_update,
	.ench    = webrtc_aec3_encode,
	.decupdh = webrtc_aec3_decode_update,
	.dech    = webrtc_aec3_decode
};

static int module_init(void)
{
	aufilt_register(baresip_aufiltl(), &webrtc_aec3);
	return 0;
}

static int module_close(void)
{
	aufilt_unregister(&webrtc_aec3);
	return 0;
}

extern "C" const struct mod_export DECL_EXPORTS(webrtc_aec3) = {
	"webrtc_aec3",
	"aufilt",
	module_init,
	module_close
};
