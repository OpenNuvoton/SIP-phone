//------------------------------------------------------------------
//
// Copyright (c) Nuvoton Technology Corp. All rights reserved.
//
//------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#include "ALSA.h"

//#define ALSA_PLAY_DEV_NAME "default"
#define ALSA_PLAY_DEV_NAME "hw:0,0"
//#define ALSA_REC_DEV_NAME "default"
#define ALSA_REC_DEV_NAME "hw:0,0"
#define REC_TEMP_BUF_BLOCK 3

////////////////////////////////////////////////////////////////////////////////
// Playback struct and variable
////////////////////////////////////////////////////////////////////////////////
typedef struct {
	int32_t i32DSPBlockSize;
} S_CTXALSA_PLAY_RES;

static pthread_mutex_t s_tALSAPlayFunMutex = PTHREAD_MUTEX_INITIALIZER;
static snd_pcm_t *s_psPlayHandle = NULL;
static int32_t s_i32PlayOpenRefCnt = 0;

//////////////////////////////////////////////////////////////////////////////////
// Record struct and variable
//////////////////////////////////////////////////////////////////////////////////

typedef struct {
	uint64_t u64FirstTimeStamp;
	uint64_t u64NextTimeStamp;
	uint32_t u32DrvSamplesPerFrame;
	int32_t i32PrevFrames;
	int32_t i32OutPrevFrames;
	uint8_t	*pu8RecvAudBuf;
	uint32_t	u32RecvAudBufSize;
	uint8_t	*pu8PreAudTempBuf;
	uint32_t u32PreAudTempBufSize;
	uint32_t u32PreAudLenInBuf;
	uint8_t	*pu8PostAudTempBuf;
	uint32_t u32PostAudTempBufSize;
	uint32_t u32PostAudLenInBuf;
	uint32_t u32ZeroAudLenInBuf;
	uint32_t u32OutSamplesPerFrame;
} S_ALSA_REC_DATA;

typedef struct{
	S_ALSA_REC_DATA sRecData;
	void *pvReserved;
}S_CTXALSA_REC_RES;

static pthread_mutex_t s_tALSARecFunMutex = PTHREAD_MUTEX_INITIALIZER;
static snd_pcm_t *s_psCaptureHandle = NULL;
static int32_t s_i32RecOpenRefCnt = 0;


//////////////////////////////////////////////////////////////////////////////////

uint64_t
Util_GetTime(void)
{
	uint64_t u64MillSec = 0;
#if 0

	double retval=0;
	char tmp[64]={0x0};
	static FILE *s_psTimeFile = NULL;

	if(s_psTimeFile == NULL){
		s_psTimeFile = fopen("/proc/uptime", "r");
		if(s_psTimeFile == NULL){			
			perror("fopen(/proc/uptime)");
			return 0;
		}
 	}
	
	while(u64MillSec == 0){
		fseek(s_psTimeFile, 0, SEEK_SET);
		fflush(s_psTimeFile);
		fgets(tmp, sizeof(tmp), s_psTimeFile);
		retval=atof(tmp);
		//fscanf
		u64MillSec = (uint64_t)(retval*1000);
		if(u64MillSec == 0){
			ERRPRINT("Get time, but return value is %f \n", retval);
		}
	}
#else
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);

	u64MillSec = (t.tv_sec * 1000) + (t.tv_nsec / 1000000);

#endif
	return u64MillSec;
}


// ==================================================
// API implement
// ==================================================

////////////////////////////////////////////////////////////////////////
//	Play
////////////////////////////////////////////////////////////////////////

static int32_t
CtxALSA_GetBlockSize(void)
{
	int i32Ret;
	snd_pcm_hw_params_t *psSndHWParam;
	uint32_t u32Channels;
	snd_pcm_uframes_t tPCMFrames;
	
	if(s_psPlayHandle == NULL)
		return 0;

	i32Ret = snd_pcm_hw_params_malloc(&psSndHWParam);
	if(i32Ret < 0){
		ERRPRINT("ALSA malloc hw params failed\n");
		return 0;
	}

	i32Ret = snd_pcm_hw_params_current(s_psPlayHandle, psSndHWParam);
	if(i32Ret < 0){
		snd_pcm_hw_params_free(psSndHWParam);
		ERRPRINT("ALSA get HW param failed\n");
		return 0;
	}

	i32Ret = snd_pcm_hw_params_get_channels(psSndHWParam, &u32Channels);
	if(i32Ret < 0){
		snd_pcm_hw_params_free(psSndHWParam);
		ERRPRINT("ALSA get HW param channels failed\n");
		return 0;
	}

	i32Ret = snd_pcm_hw_params_get_buffer_size(psSndHWParam, &tPCMFrames);
	if(i32Ret < 0){
		snd_pcm_hw_params_free(psSndHWParam);
		ERRPRINT("ALSA get HW param buffer size failed\n");
		return 0;
	}

	snd_pcm_hw_params_free(psSndHWParam);
	return (tPCMFrames * sizeof(uint16_t) * u32Channels);
}

ERRCODE
ALSA_PlayOpen(
	S_NM_AUDIOCTX	*psCtx,				// [in] Audio context.
	void			**ppCtxRes			// [out] ALSA context resource
)
{
	S_CTXALSA_PLAY_RES *psCtxALSARes = NULL;
	ERRCODE eRetCode = ERR_ALSA_NONE;
	int i32Ret;
	snd_pcm_hw_params_t *psSndHWParam = NULL;

	FUN_MUTEX_LOCK(&s_tALSAPlayFunMutex);
	*ppCtxRes = NULL;

	if (psCtx == NULL) {
		FUN_MUTEX_UNLOCK(&s_tALSAPlayFunMutex);
		return ERR_ALSA_CTX;
	}

	psCtxALSARes = Util_Calloc(1, sizeof(S_CTXALSA_PLAY_RES));

	if (psCtxALSARes == NULL) {
		FUN_MUTEX_UNLOCK(&s_tALSAPlayFunMutex);
		return ERR_ALSA_RES;
	}

	if(!s_i32PlayOpenRefCnt){
		i32Ret = snd_pcm_open(&s_psPlayHandle, ALSA_PLAY_DEV_NAME, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
		if(i32Ret < 0){
			ERRPRINT("ALSA %s open failed \n", ALSA_PLAY_DEV_NAME);
			eRetCode = ERR_ALSA_DEV;
			goto ALSA_PlayOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params_malloc(&psSndHWParam);
		if(i32Ret < 0){
			ERRPRINT("ALSA malloc hw params failed \n");
			eRetCode = ERR_ALSA_MALLOC;
			goto ALSA_PlayOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params_any(s_psPlayHandle, psSndHWParam);
		if(i32Ret < 0){
			ERRPRINT("ALSA initialize hw params failed \n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_PlayOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params_set_access(s_psPlayHandle, psSndHWParam, SND_PCM_ACCESS_RW_INTERLEAVED);
		if(i32Ret < 0){
			ERRPRINT("ALSA set access type failed\n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_PlayOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params_set_format(s_psPlayHandle, psSndHWParam, SND_PCM_FORMAT_S16_LE);
		if(i32Ret < 0){
			ERRPRINT("ALSA set format failed\n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_PlayOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params_set_rate_near(s_psPlayHandle, psSndHWParam, &(psCtx->u32SampleRate), 0);
		if(i32Ret < 0){
			ERRPRINT("ALSA set sample rate failed\n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_PlayOpen_Fail;
		}

		snd_pcm_hw_params_get_rate(psSndHWParam, &(psCtx->u32SampleRate), 0);

		i32Ret = snd_pcm_hw_params_set_channels(s_psPlayHandle, psSndHWParam, psCtx->u32ChannelNum);
		if(i32Ret < 0){
			ERRPRINT("ALSA set sample channel failed\n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_PlayOpen_Fail;
		}
		i32Ret = snd_pcm_hw_params(s_psPlayHandle, psSndHWParam);
		if(i32Ret < 0){
			ERRPRINT("ALSA set hardware parameters failed \n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_PlayOpen_Fail;
		}

	}

	//TODO: set mixer volume
	
	if(!s_i32PlayOpenRefCnt){
		if(psSndHWParam){
			snd_pcm_hw_params_free(psSndHWParam);
		}
		
		i32Ret = snd_pcm_prepare(s_psPlayHandle);
		if(i32Ret < 0){
			ERRPRINT("ALSA prepare audio interface failed \n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_PlayOpen_Fail;
		}

#if 0
		i32Ret = snd_pcm_start(s_psPlayHandle);
		if(i32Ret < 0){
			ERRPRINT("ALSA start audio interface failed %s \n", snd_strerror(i32Ret));
			eRetCode = ERR_ALSA_IO;
			goto ALSA_PlayOpen_Fail;
		}
#endif
	}

	psCtxALSARes->i32DSPBlockSize = CtxALSA_GetBlockSize();
	s_i32PlayOpenRefCnt ++;
	*ppCtxRes = psCtxALSARes;

	DEBUGPRINT("ALSA play open done \n");
	FUN_MUTEX_UNLOCK(&s_tALSAPlayFunMutex);
	return eRetCode;

ALSA_PlayOpen_Fail:

	if((!s_i32PlayOpenRefCnt) && (s_psPlayHandle)){
		snd_pcm_close(s_psPlayHandle);
		s_psPlayHandle = NULL;
	}

	if(psCtxALSARes)
		Util_Free(psCtxALSARes, sizeof(S_CTXALSA_PLAY_RES));

	DEBUGPRINT("ALSA play open failed \n");
	FUN_MUTEX_UNLOCK(&s_tALSAPlayFunMutex);
	return eRetCode;

}

ERRCODE
ALSA_PlayClose(
	void			**ppCtxRes			// [out] ALSA context resource
)
{
	S_CTXALSA_PLAY_RES *psCtxALSARes;

	if (*ppCtxRes == NULL)
		return ERR_ALSA_RES;

	psCtxALSARes = (S_CTXALSA_PLAY_RES *)(*ppCtxRes);

	FUN_MUTEX_LOCK(&s_tALSAPlayFunMutex);

	s_i32PlayOpenRefCnt --;

	if(s_i32PlayOpenRefCnt < 0){
		s_i32PlayOpenRefCnt = 0;
	}

	if(!s_i32PlayOpenRefCnt){
		if(s_psPlayHandle){
			snd_pcm_close(s_psPlayHandle);
			s_psPlayHandle = NULL;
		}
	}

	if(psCtxALSARes)
		Util_Free(psCtxALSARes, sizeof(S_CTXALSA_PLAY_RES));

	DEBUGPRINT("ALSA play closed \n");

	*ppCtxRes = NULL;
	FUN_MUTEX_UNLOCK(&s_tALSAPlayFunMutex);
	return ERR_ALSA_NONE;
}

ERRCODE
ALSA_PlayWrite(
	S_NM_AUDIOCTX	*psCtx,				// [in] Audio context.
	uint32_t		*pu32WrittenByte,	// [out] Written bytes
	void			*pCtxRes			// [in] ALSA context resource
)
{
	S_CTXALSA_PLAY_RES *psCtxRes;

	if(pCtxRes == NULL)
		return ERR_ALSA_RES;
	
	if(psCtx == NULL)
		return ERR_ALSA_CTX;

	psCtxRes = (S_CTXALSA_PLAY_RES *)pCtxRes;

	FUN_MUTEX_LOCK(&s_tALSAPlayFunMutex);

	if(s_psPlayHandle == NULL){
		FUN_MUTEX_UNLOCK(&s_tALSAPlayFunMutex);
		return ERR_ALSA_DEV;
	}

	snd_pcm_sframes_t tAvailFrames;
	uint32_t u32AvailData;
	int32_t i32DataWrite;
	snd_pcm_uframes_t t32FrameWrite;
	
	*pu32WrittenByte = 0;
	tAvailFrames = snd_pcm_avail(s_psPlayHandle); //alsa frame: 1 sample per frame
	
	if(tAvailFrames == -EPIPE)
		snd_pcm_prepare(s_psPlayHandle);

	if(tAvailFrames <= 0){
		DEBUGPRINT("DDDDD ALSA_PlayWrite snd_pcm_avail tAvailFrames %d \n", (int)tAvailFrames);
		u32AvailData = psCtx->u32DataSize;
	}
	else{
		u32AvailData = tAvailFrames * sizeof(uint16_t) * psCtx->u32ChannelNum;
	}

	i32DataWrite = psCtx->u32DataSize;
	if ((int32_t)psCtx->u32DataSize > psCtxRes->i32DSPBlockSize) {
		if (psCtxRes->i32DSPBlockSize > u32AvailData)
			i32DataWrite = u32AvailData;
		else
			i32DataWrite = psCtxRes->i32DSPBlockSize;
	}
	else if (u32AvailData < (int32_t)psCtx->u32DataSize) {
		i32DataWrite = u32AvailData;
	}

	if(i32DataWrite < psCtx->u32DataSize){
		//Wait for available frame size > context data size
		FUN_MUTEX_UNLOCK(&s_tALSAPlayFunMutex);
		return ERR_ALSA_NONE;
	}

	t32FrameWrite = i32DataWrite / sizeof(uint16_t) / psCtx->u32ChannelNum;
	tAvailFrames = snd_pcm_writei(s_psPlayHandle, psCtx->pDataVAddr, t32FrameWrite);
//	DEBUGPRINT("DDDDD ALSA_PlayWrite tAvailFrames %d \n", tAvailFrames);

#if 1
	
    if (snd_pcm_state(s_psPlayHandle) == SND_PCM_STATE_PREPARED) {
		int err;
        err = snd_pcm_start(s_psPlayHandle);
		DEBUGPRINT("Start error: %s\n", snd_strerror(err));
	}

#endif

	if(tAvailFrames  < 0){
		FUN_MUTEX_UNLOCK(&s_tALSAPlayFunMutex);
		return ERR_ALSA_IO;
	}


	*pu32WrittenByte = tAvailFrames * sizeof(uint16_t) * psCtx->u32ChannelNum;
	FUN_MUTEX_UNLOCK(&s_tALSAPlayFunMutex);
	return ERR_ALSA_NONE;
}

////////////////////////////////////////////////////////////////////////
//	Record
////////////////////////////////////////////////////////////////////////

#if 1
	uint64_t s_u64RecCalTime = 0;
	uint32_t s_u32RecBytes = 0;
#endif

static void
PutAudioToBuf(
	uint8_t *pu8AudData,
	uint32_t u32DataLen,
	S_ALSA_REC_DATA *psRecData
) {
	if (psRecData->u32ZeroAudLenInBuf) { //put it in post buffer
		if(psRecData->u32PostAudLenInBuf >= psRecData->u32PostAudTempBufSize){
			DEBUGPRINT("Over pu8PostAudTempBuf, psRecData->u32PostAudLenInBuf %d \n", psRecData->u32PostAudLenInBuf);
			return;
		}

		memcpy((psRecData->pu8PostAudTempBuf + psRecData->u32PostAudLenInBuf), pu8AudData, u32DataLen);
		psRecData->u32PostAudLenInBuf += u32DataLen;
		return;
	}

	if(psRecData->u32PreAudLenInBuf >= psRecData->u32PreAudTempBufSize){
		DEBUGPRINT("Over pu8PreAudTempBuf, psRecData->u32PreAudLenInBuf %d \n", psRecData->u32PreAudLenInBuf);
		return;
	}

	memcpy((psRecData->pu8PreAudTempBuf + psRecData->u32PreAudLenInBuf), pu8AudData, u32DataLen);
	psRecData->u32PreAudLenInBuf += u32DataLen;
}


static bool
GetAudioFromBuf(
	uint8_t *pu8AudData,
	uint32_t u32DataLen,
	uint64_t *pu64TimeStamp,
	uint32_t u32Channels,
	uint32_t u32SampleRate,
	S_ALSA_REC_DATA *psRecData
) {
	uint32_t u32TotalBytesInBuf;
	uint32_t u32TotalSamplesInBuf;

	u32TotalBytesInBuf = psRecData->u32PreAudLenInBuf + psRecData->u32ZeroAudLenInBuf + psRecData->u32PostAudLenInBuf;
	u32TotalSamplesInBuf = u32TotalBytesInBuf / sizeof(uint16_t) / u32Channels;

	if(u32TotalBytesInBuf < u32DataLen){
		return false;
	}

	double dBDelayTime;
	uint64_t u64CurTime;
	
	u64CurTime = Util_GetTime();
	dBDelayTime = (double)(u32TotalSamplesInBuf * 1000) / (double)(u32SampleRate);
	u64CurTime -= (uint64_t)dBDelayTime;

	uint32_t u32GetedLen = 0;
	uint8_t *pu8TempBuf = NULL;
	uint32_t *pu32TempBufLen = 0;
	uint32_t u32TempBufLen = 0;
	uint32_t u32CopyLen;

	while(u32GetedLen < u32DataLen){
		if(psRecData->u32PreAudLenInBuf){
			if ((psRecData->u32ZeroAudLenInBuf == 0) &&
					(psRecData->u32PostAudLenInBuf)){
				pu8TempBuf = psRecData->pu8PostAudTempBuf;
				pu32TempBufLen = &psRecData->u32PostAudLenInBuf;
			}
			else{
				pu8TempBuf = psRecData->pu8PreAudTempBuf;
				pu32TempBufLen = &psRecData->u32PreAudLenInBuf;
			}
		}
		else if(psRecData->u32ZeroAudLenInBuf){
			pu8TempBuf = NULL;
			pu32TempBufLen = &psRecData->u32ZeroAudLenInBuf;
		}
		else{
			pu8TempBuf = psRecData->pu8PostAudTempBuf;
			pu32TempBufLen = &psRecData->u32PostAudLenInBuf;
		}

		u32TempBufLen = *pu32TempBufLen;
		u32CopyLen = u32DataLen - u32GetedLen;

		if (u32CopyLen > u32TempBufLen) {
			u32CopyLen = u32TempBufLen;
		}

		if (pu8TempBuf) {
			memcpy((pu8AudData + u32GetedLen),  pu8TempBuf, u32CopyLen);
			u32TempBufLen -= u32CopyLen;

			if (u32TempBufLen) {
				memcpy(pu8TempBuf, pu8TempBuf + u32CopyLen, u32TempBufLen);
			}

			*pu32TempBufLen = u32TempBufLen;
		}
		else {
			memset((pu8AudData + u32GetedLen),  0x0, u32CopyLen);
			u32TempBufLen -= u32CopyLen;
			*pu32TempBufLen = u32TempBufLen;
		}

		u32GetedLen += u32CopyLen;
	}

	if (u64CurTime < psRecData->u64FirstTimeStamp)
		u64CurTime = psRecData->u64FirstTimeStamp;

	double dSecondPerFrame;
	uint32_t u32Frames;
	
	dSecondPerFrame = (double)(psRecData->u32OutSamplesPerFrame * 1000) / (double)(u32SampleRate);
	u32Frames = (uint32_t)((double)(u64CurTime - psRecData->u64FirstTimeStamp) / dSecondPerFrame);

#if 0
	DEBUGPRINT(" u32Frames %d, i32OutPrevFrames %d \n", u32Frames, psRecData->i32OutPrevFrames);
	if (u32Frames == (psRecData->i32OutPrevFrames)) {
		u32Frames = (psRecData->i32OutPrevFrames + 1);
	}

	if (u32Frames < (psRecData->i32OutPrevFrames + 1)) {
		DEBUGPRINT("Discard audio data u32Frames %d, i32OutPrevFrames %d \n", u32Frames, psRecData->i32OutPrevFrames);
		return false;
	}

	if (u32Frames > (psRecData->i32OutPrevFrames + 10)) {
		DEBUGPRINT("u32Frames %d, i32OutPrevFrames %d \n", u32Frames, psRecData->i32OutPrevFrames);
		DEBUGPRINT("u64CurTime %"PRId64"\n", u64CurTime);
		DEBUGPRINT("psRecData->u64FirstTimeStamp %"PRId64" \n", psRecData->u64FirstTimeStamp);
		DEBUGPRINT("psRecData->u32OutSamplesPerFrame %d \n", psRecData->u32OutSamplesPerFrame);
		DEBUGPRINT("u32TotalBytesInBuf %d \n", u32TotalBytesInBuf);
	}
#else
	if (u32Frames <= (psRecData->i32OutPrevFrames)) {
		u32Frames = (psRecData->i32OutPrevFrames + 1);
	}
#endif

	if(s_u64RecCalTime == 0)
		s_u64RecCalTime = Util_GetTime();

	s_u32RecBytes += u32DataLen;

	if(Util_GetTime() > (s_u64RecCalTime + 10000))
	{
		DEBUGPRINT("Rec Read data %d bytes, psRecData->u32DrvSamplesPerFrame %d\n", s_u32RecBytes, psRecData->u32DrvSamplesPerFrame);
		s_u32RecBytes = 0;
		s_u64RecCalTime = Util_GetTime();
	}

	u64CurTime = psRecData->u64FirstTimeStamp + (uint64_t)(u32Frames * dSecondPerFrame);
	psRecData->i32OutPrevFrames = u32Frames;
	*pu64TimeStamp = u64CurTime;
	return true;
}




ERRCODE
ALSA_RecOpen(
	uint32_t u32FragmentMS,				// [in] Audio fragment millisecond
	S_NM_AUDIOCTX	*psCtx,				// [in] Audio context.
	void			**ppCtxRes			// [out] ALSA context resource
)
{
	S_CTXALSA_REC_RES *psCtxALSARes = NULL;
	ERRCODE eRetCode = ERR_ALSA_NONE;
	int i32Ret;
	snd_pcm_hw_params_t *psSndHWParam = NULL;


	FUN_MUTEX_LOCK(&s_tALSARecFunMutex);
	*ppCtxRes = NULL;

	if (psCtx == NULL) {
		FUN_MUTEX_UNLOCK(&s_tALSARecFunMutex);
		return ERR_ALSA_CTX;
	}
	
	psCtxALSARes = Util_Calloc(1, sizeof(S_CTXALSA_REC_RES));

	if (psCtxALSARes == NULL) {
		FUN_MUTEX_UNLOCK(&s_tALSARecFunMutex);
		return ERR_ALSA_RES;
	}

	if(!s_i32RecOpenRefCnt){

		i32Ret = snd_pcm_open(&s_psCaptureHandle, ALSA_REC_DEV_NAME, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
		if(i32Ret < 0){
			ERRPRINT("ALSA %s open failed \n", ALSA_REC_DEV_NAME);
			eRetCode = ERR_ALSA_DEV;
			goto ALSA_RecOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params_malloc(&psSndHWParam);
		if(i32Ret < 0){
			ERRPRINT("ALSA malloc hw params failed \n");
			eRetCode = ERR_ALSA_MALLOC;
			goto ALSA_RecOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params_any(s_psCaptureHandle, psSndHWParam);
		if(i32Ret < 0){
			ERRPRINT("ALSA initialize hw params failed \n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_RecOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params_set_access(s_psCaptureHandle, psSndHWParam, SND_PCM_ACCESS_RW_INTERLEAVED);
		if(i32Ret < 0){
			ERRPRINT("ALSA set access type failed \n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_RecOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params_set_format(s_psCaptureHandle, psSndHWParam, SND_PCM_FORMAT_S16_LE);
		if(i32Ret < 0){
			ERRPRINT("ALSA set format failed \n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_RecOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params_set_rate_near(s_psCaptureHandle, psSndHWParam, &(psCtx->u32SampleRate), 0);
		if(i32Ret < 0){
			ERRPRINT("ALSA set sample rate failed \n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_RecOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params_set_channels(s_psCaptureHandle, psSndHWParam, psCtx->u32ChannelNum);
		if(i32Ret < 0){
			ERRPRINT("ALSA set sample channel failed \n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_RecOpen_Fail;
		}

		i32Ret = snd_pcm_hw_params(s_psCaptureHandle, psSndHWParam);
		if(i32Ret < 0){
			ERRPRINT("ALSA set hardware parameters failed \n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_RecOpen_Fail;
		}
	}

	//FixMe: To change u32DrvSamplesPerFrame for audio data fragment size
	psCtxALSARes->sRecData.u32OutSamplesPerFrame = (psCtx->u32SampleRate * u32FragmentMS) / 1000;
	psCtxALSARes->sRecData.u32DrvSamplesPerFrame = (psCtx->u32SampleRate * u32FragmentMS) / 1000;

	DEBUGPRINT("ALSA samples rate %d, App fragment %d \n", psCtx->u32SampleRate, u32FragmentMS);


	psCtxALSARes->sRecData.u32RecvAudBufSize = psCtxALSARes->sRecData.u32DrvSamplesPerFrame * psCtx->u32ChannelNum * sizeof(int16_t);
	psCtxALSARes->sRecData.pu8RecvAudBuf = Util_Malloc(psCtxALSARes->sRecData.u32RecvAudBufSize);
	if(psCtxALSARes->sRecData.pu8RecvAudBuf == NULL) {
		ERRPRINT("ALSA allocate audio receive buffer failed \n");
		eRetCode = ERR_ALSA_MALLOC;
		goto ALSA_RecOpen_Fail;
	}

	if (psCtxALSARes->sRecData.u32OutSamplesPerFrame > psCtxALSARes->sRecData.u32DrvSamplesPerFrame){
		psCtxALSARes->sRecData.u32PreAudTempBufSize = psCtxALSARes->sRecData.u32OutSamplesPerFrame * psCtx->u32ChannelNum * sizeof(int16_t) * REC_TEMP_BUF_BLOCK;
	}
	else{
		psCtxALSARes->sRecData.u32PreAudTempBufSize = psCtxALSARes->sRecData.u32DrvSamplesPerFrame * psCtx->u32ChannelNum * sizeof(int16_t) * REC_TEMP_BUF_BLOCK;
	}

	psCtxALSARes->sRecData.u32PostAudTempBufSize = psCtxALSARes->sRecData.u32PreAudTempBufSize;

	psCtxALSARes->sRecData.pu8PreAudTempBuf = Util_Malloc(psCtxALSARes->sRecData.u32PreAudTempBufSize);
	if(psCtxALSARes->sRecData.pu8PreAudTempBuf == NULL){
		ERRPRINT("ALSA allocate audio pre temp buffer failed \n");
		eRetCode = ERR_ALSA_MALLOC;
		goto ALSA_RecOpen_Fail;
	}

	psCtxALSARes->sRecData.u32PreAudLenInBuf = 0;

	psCtxALSARes->sRecData.pu8PostAudTempBuf = Util_Malloc(psCtxALSARes->sRecData.u32PostAudTempBufSize);
	if(psCtxALSARes->sRecData.pu8PostAudTempBuf == NULL){
		ERRPRINT("ALSA allocate audio post temp buffer failed \n");
		eRetCode = ERR_ALSA_MALLOC;
		goto ALSA_RecOpen_Fail;
	}

	psCtxALSARes->sRecData.u32PostAudLenInBuf = 0;

	if(!s_i32RecOpenRefCnt){
		if(psSndHWParam){
			snd_pcm_hw_params_free(psSndHWParam);
		}
		
		i32Ret = snd_pcm_prepare(s_psCaptureHandle);
		if(i32Ret < 0){
			ERRPRINT("ALSA prepare audio interface failed \n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_RecOpen_Fail;
		}

		i32Ret = snd_pcm_start(s_psCaptureHandle);
		if(i32Ret < 0){
			ERRPRINT("ALSA start audio interface failed \n");
			eRetCode = ERR_ALSA_IO;
			goto ALSA_RecOpen_Fail;
		}
	}

	psCtxALSARes->sRecData.u32ZeroAudLenInBuf = 0;
	psCtxALSARes->sRecData.u64FirstTimeStamp = 0;
	psCtxALSARes->sRecData.i32PrevFrames = -1;
	psCtxALSARes->sRecData.i32OutPrevFrames = -1;
	DEBUGPRINT("ALSA fragment samples %d, App frames samples %d \n", psCtxALSARes->sRecData.u32DrvSamplesPerFrame, psCtxALSARes->sRecData.u32OutSamplesPerFrame);

	psCtx->u32SampleNum = psCtxALSARes->sRecData.u32OutSamplesPerFrame;  //2:PCM_S16LE
	psCtx->u32DataSize = psCtx->u32SampleNum * psCtx->u32ChannelNum * sizeof(uint16_t);
	psCtx->u32DataLimit = psCtx->u32DataSize;
	s_i32RecOpenRefCnt ++;
	*ppCtxRes = psCtxALSARes;

	DEBUGPRINT("ALSA record open done \n");
	FUN_MUTEX_UNLOCK(&s_tALSARecFunMutex);
	return eRetCode;

ALSA_RecOpen_Fail:

	if(psSndHWParam){
		snd_pcm_hw_params_free(psSndHWParam);
	}

	if(psCtxALSARes->sRecData.pu8PostAudTempBuf){
		Util_Free(psCtxALSARes->sRecData.pu8PostAudTempBuf, psCtxALSARes->sRecData.u32PostAudTempBufSize);
	}

	if(psCtxALSARes->sRecData.pu8PreAudTempBuf){
		Util_Free(psCtxALSARes->sRecData.pu8PreAudTempBuf, psCtxALSARes->sRecData.u32PreAudTempBufSize);
	}

	if(psCtxALSARes->sRecData.pu8RecvAudBuf)
		Util_Free(psCtxALSARes->sRecData.pu8RecvAudBuf, psCtxALSARes->sRecData.u32RecvAudBufSize);

	if((!s_i32RecOpenRefCnt) && (s_psCaptureHandle)){
		snd_pcm_close(s_psCaptureHandle);
		s_psCaptureHandle = NULL;
	}

	if(psCtxALSARes)
		Util_Free(psCtxALSARes, sizeof(S_CTXALSA_REC_RES));

	DEBUGPRINT("ALSA record open failed \n");
	FUN_MUTEX_UNLOCK(&s_tALSARecFunMutex);
	return eRetCode;
}

ERRCODE
ALSA_RecClose(
	void			**ppCtxRes			// [out] OSS context resource
) 
{
	S_CTXALSA_REC_RES *psCtxALSARes;

	if (*ppCtxRes == NULL)
		return ERR_ALSA_RES;

	psCtxALSARes = (S_CTXALSA_REC_RES *)(*ppCtxRes);

	FUN_MUTEX_LOCK(&s_tALSARecFunMutex);

	if(psCtxALSARes->sRecData.pu8PostAudTempBuf){
		Util_Free(psCtxALSARes->sRecData.pu8PostAudTempBuf, psCtxALSARes->sRecData.u32PostAudTempBufSize);
	}

	if(psCtxALSARes->sRecData.pu8PreAudTempBuf){
		Util_Free(psCtxALSARes->sRecData.pu8PreAudTempBuf, psCtxALSARes->sRecData.u32PreAudTempBufSize);
	}

	if(psCtxALSARes->sRecData.pu8RecvAudBuf)
		Util_Free(psCtxALSARes->sRecData.pu8RecvAudBuf, psCtxALSARes->sRecData.u32RecvAudBufSize);

	s_i32RecOpenRefCnt --;

	if (s_i32RecOpenRefCnt < 0) {
		s_i32RecOpenRefCnt = 0;
	}

	if(!s_i32RecOpenRefCnt){
		if(s_psCaptureHandle){
			snd_pcm_close(s_psCaptureHandle);
			s_psCaptureHandle = NULL;
		}
	}

	if(psCtxALSARes)
		Util_Free(psCtxALSARes, sizeof(S_CTXALSA_REC_RES));

	DEBUGPRINT("ALSA record closed \n");

	*ppCtxRes = NULL;
	FUN_MUTEX_UNLOCK(&s_tALSARecFunMutex);
	return ERR_ALSA_NONE;
}

ERRCODE
ALSA_RecRead(
	S_NM_AUDIOCTX	*psCtx,				// [in] Audio context.
	void			*pCtxRes			// [in] OSS context resource
)
{
	S_ALSA_REC_DATA *psRecData;

	if (pCtxRes == NULL) {
		return ERR_ALSA_RES;
	}

	FUN_MUTEX_LOCK(&s_tALSARecFunMutex);
	psCtx->u32DataSize = 0;

	if (s_psCaptureHandle == NULL) {
		FUN_MUTEX_UNLOCK(&s_tALSARecFunMutex);
		return ERR_ALSA_DEV;
	}

	uint32_t u32CurSamplesInBuf;
	S_CTXALSA_REC_RES *psCtxRes = (S_CTXALSA_REC_RES *)pCtxRes;
	snd_pcm_sframes_t tAvailFrames;

	psRecData = &psCtxRes->sRecData;
	u32CurSamplesInBuf = (psRecData->u32PreAudLenInBuf + psRecData->u32ZeroAudLenInBuf + psRecData->u32PostAudLenInBuf) / sizeof(uint16_t) / psCtx->u32ChannelNum;

	tAvailFrames = snd_pcm_avail(s_psCaptureHandle); //alsa frame: 1 sample per frame

	if(tAvailFrames < psRecData->u32DrvSamplesPerFrame){
		if(tAvailFrames < 0){
			printf("DDDDD ALSA_RecRead snd_pcm_avail fail tAvailFrames %ld \n", tAvailFrames);
		}
		else if(u32CurSamplesInBuf < psRecData->u32OutSamplesPerFrame){
			FUN_MUTEX_UNLOCK(&s_tALSARecFunMutex);
			return ERR_ALSA_NOT_READY;
		}
		else{
			goto alsa_get_audio;
		}
	}

	tAvailFrames = snd_pcm_readi(s_psCaptureHandle, psRecData->pu8RecvAudBuf, psRecData->u32DrvSamplesPerFrame);

	if(tAvailFrames < 0){
		if(u32CurSamplesInBuf < psRecData->u32OutSamplesPerFrame){
			printf("DDDDD ALSA_RecRead snd_pcm_readi tAvailFrames %ld \n", tAvailFrames);
			FUN_MUTEX_UNLOCK(&s_tALSARecFunMutex);
			return ERR_ALSA_NOT_READY;
		}
		else{
			goto alsa_get_audio;
		}
	}

	uint64_t u64CurTime = Util_GetTime();
	double dBDelayTime;
	uint32_t u32AvailData = tAvailFrames * sizeof(uint16_t) * psCtx->u32ChannelNum;

	dBDelayTime = (double)(tAvailFrames * 1000) / (double)(psCtx->u32SampleRate);
	u64CurTime -= (uint64_t) dBDelayTime;

	if (psRecData->u64FirstTimeStamp == 0) {
		psRecData->u64FirstTimeStamp = u64CurTime;
		psRecData->u64NextTimeStamp = u64CurTime;
	}

#if 0
	double dSecondPerFrame;
	int32_t i32Frames;
	
	dSecondPerFrame = (double)(psRecData->u32DrvSamplesPerFrame * 1000) / (double)(psCtx->u32SampleRate);
	i32Frames = (uint32_t)((double)(u64CurTime - psRecData->u64FirstTimeStamp) / dSecondPerFrame);

	if (i32Frames == (psRecData->i32PrevFrames)) {
		i32Frames = (psRecData->i32PrevFrames + 1);
	}

	if (i32Frames >= (psRecData->i32PrevFrames + 1)) {
		if ((i32Frames - (psRecData->i32PrevFrames)) > 1) { //lost audio data, fill 0 into buffer
			DEBUGPRINT("Aud Frames %d,  Prev frames %d \n", i32Frames, psRecData->i32PrevFrames);

			if (psRecData->u32PostAudLenInBuf) {
				DEBUGPRINT("Skip zero data");
				psRecData->i32PrevFrames = i32Frames;
				goto alsa_get_audio;
			}

			psRecData->u32ZeroAudLenInBuf = (i32Frames - (psRecData->i32PrevFrames + 1)) * (psRecData->u32DrvSamplesPerFrame * sizeof(uint16_t) * psCtx->u32ChannelNum);
		}
		psRecData->i32PrevFrames = i32Frames;
		// put data into buffer
		PutAudioToBuf(psRecData->pu8RecvAudBuf, u32AvailData, psRecData);
	}
	else {
		DEBUGPRINT("discard audio data %d %d %"PRId64" \n", i32Frames, psRecData->i32PrevFrames, u64CurTime);
	}
#else

	PutAudioToBuf(psRecData->pu8RecvAudBuf, u32AvailData, psRecData);
#endif

	uint32_t u32OutFrameSize;

alsa_get_audio:
	
	u32OutFrameSize = psRecData->u32OutSamplesPerFrame * sizeof(uint16_t) * psCtx->u32ChannelNum;

	if(GetAudioFromBuf((uint8_t *)psCtx->pDataVAddr ,
						u32OutFrameSize, 
						&psCtx->u64DataTime,
						psCtx->u32ChannelNum, 
						psCtx->u32SampleRate,
						psRecData) == false) {
		FUN_MUTEX_UNLOCK(&s_tALSARecFunMutex);
		return ERR_ALSA_NOT_READY;
	}

	psCtx->u32DataSize = u32OutFrameSize;
	FUN_MUTEX_UNLOCK(&s_tALSARecFunMutex);
	return ERR_ALSA_NONE;
}
