#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <inttypes.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <sys/time.h>

#include "echo_control_mobile.h"
#include "ALSA.h"

//#define REC_ONLY_TEST

#define AUDIO_SR 8000
#define WEBRTC_FAREND_BUF_SAMPLES  80 // number of samples of webrtc farend buffer

#define AUDIO_REC_FRAG ((WEBRTC_FAREND_BUF_SAMPLES * 1000) / AUDIO_SR) //10 millisecond
#define DEF_PCM_BUF_CNT 	6		//Default PCM buffer count.

static bool s_bThreadRun = false;
static bool s_bEnEchoCancel = false;

static S_NM_AUDIOCTX s_sAudioRecCtx;
static S_NM_AUDIOCTX s_sAudioPlayCtx;
static void *s_pvAudioRecRes = NULL;
static void *s_pvAudioPlayRes= NULL;
static uint32_t s_u32RecFrameSize;
static uint32_t s_u32PlayFrameSize;

static uint32_t s_u32PCMBufSize;				//Audio PCM buffer size
static uint8_t *s_pu8PCMBuf = NULL;					//Pointer to audio PCM buffer
static uint32_t s_u32PCMInIdx;					//Audio PCM input index
static uint32_t s_u32PCMOutIdx;					//Audio PCM output index

static pthread_mutex_t s_hPCMBufMutex;

void *s_pvAECMInst = NULL;

using namespace webrtc;

static uint32_t 
GetTimeMilliSec(void)
{
	uint32_t u32CurTime;
	struct timeval sTimeVal;
	
	gettimeofday(&sTimeVal, NULL);

	u32CurTime = (sTimeVal.tv_sec * 1000) + (sTimeVal.tv_usec / 1000);
	return u32CurTime;
}

static uint32_t 
GetTimeMicroSec(void)
{
	uint32_t u32CurTime;
	struct timeval sTimeVal;
	
	gettimeofday(&sTimeVal, NULL);

	u32CurTime = (sTimeVal.tv_sec * 1000000) + (sTimeVal.tv_usec);
	return u32CurTime;
}


//Audio record thread
static void *AudioRecThread(
	void *argu
)
{
	ERRCODE eErr;
	uint8_t *pu8TempRecBuf;
	uint32_t u32StartOPTime;
	uint32_t u32EndOPTime;
	int32_t i32WebRTCRet;

	pu8TempRecBuf = (uint8_t *)malloc(s_u32RecFrameSize);
	if(pu8TempRecBuf == NULL){
		ERRPRINT("Cannot alloc temp record buffer \n");
		return 0;
	}
	//featch record data, and store to buffer
	while(s_bThreadRun){
		// lock pcm buffer
		pthread_mutex_lock(&s_hPCMBufMutex);
		// adjust pcm in index
		if((s_u32PCMInIdx + s_u32RecFrameSize) >= (s_u32PCMBufSize - (s_u32RecFrameSize * 2))){
			s_u32PCMInIdx = s_u32PCMInIdx - s_u32PCMOutIdx;
			memcpy(s_pu8PCMBuf, s_pu8PCMBuf + s_u32PCMOutIdx, s_u32PCMInIdx);
			s_u32PCMOutIdx = 0;
		} 
		
		if((s_u32PCMInIdx + s_u32RecFrameSize) > s_u32PCMBufSize){
			DEBUGPRINT("PCM buffer is too small, discard old data \n");
			s_u32PCMInIdx = 0;
			s_u32PCMOutIdx = 0;
		}

		s_sAudioRecCtx.pDataVAddr = pu8TempRecBuf;
		s_sAudioRecCtx.u32DataLimit = s_u32RecFrameSize;

		// Read audio data
		eErr = ALSA_RecRead(&s_sAudioRecCtx, s_pvAudioRecRes);

		if(eErr == ERR_ALSA_NONE){

			if (s_bEnEchoCancel){
				u32StartOPTime = GetTimeMicroSec();

				i32WebRTCRet = WebRtcAecm_Process(s_pvAECMInst,
													(int16_t *)pu8TempRecBuf,
													NULL,
													(int16_t *)(s_pu8PCMBuf + s_u32PCMInIdx),
													(s_sAudioRecCtx.u32DataSize / s_sAudioRecCtx.u32ChannelNum), 
													AUDIO_REC_FRAG);

				u32EndOPTime = GetTimeMicroSec();
				if(i32WebRTCRet!=0){
					DEBUGPRINT("WebRtcAecm_Process error %d\n", i32WebRTCRet);
				}

				DEBUGPRINT("echo cancel spend time is %d us \n", u32EndOPTime - u32StartOPTime);
				s_u32PCMInIdx += s_sAudioRecCtx.u32DataSize;				
			}
			else{
				memcpy((s_pu8PCMBuf + s_u32PCMInIdx), pu8TempRecBuf, s_sAudioRecCtx.u32DataSize);
				s_u32PCMInIdx += s_sAudioRecCtx.u32DataSize;
			}
		}

		// unlock PCM mutex
		pthread_mutex_unlock(&s_hPCMBufMutex);
		usleep(1000);
	}
	return 0;
}

//Audio play thread
static void *AudioPlayThread(
	void *argu
)
{
	uint32_t u32PlayByte;
	int32_t i32WebRTCRet;

	//get audio date form buffer and play it
	while(s_bThreadRun){
		// lock pcm buffer
		pthread_mutex_lock(&s_hPCMBufMutex);

		if((s_u32PCMInIdx - s_u32PCMOutIdx) < s_u32PlayFrameSize){
			// unlock PCM mutex
			pthread_mutex_unlock(&s_hPCMBufMutex);
			usleep(1000);
			continue;
		}

		s_sAudioPlayCtx.pDataVAddr = s_pu8PCMBuf + s_u32PCMOutIdx;
		s_sAudioPlayCtx.u32DataSize = s_u32PlayFrameSize;
		s_sAudioPlayCtx.u32DataLimit = s_u32PlayFrameSize;

		u32PlayByte = 0;

		ALSA_PlayWrite(&s_sAudioPlayCtx, &u32PlayByte, s_pvAudioPlayRes);

		if(u32PlayByte){

			if(s_bEnEchoCancel == true){
				i32WebRTCRet = WebRtcAecm_BufferFarend(s_pvAECMInst, (int16_t*)s_sAudioPlayCtx.pDataVAddr, WEBRTC_FAREND_BUF_SAMPLES * s_sAudioPlayCtx.u32ChannelNum);

				if(i32WebRTCRet!=0){
					DEBUGPRINT("WebRtcAecm_BufferFarend error %d\n", i32WebRTCRet);
				}
			}

			if(u32PlayByte != s_u32PlayFrameSize){
				DEBUGPRINT("Audio play u32PlayByte!= s_u32PlayFrameSize\n");
			}
//			else{
//				DEBUGPRINT("Audio play u32PlayByte== s_u32PlayFrameSize\n");
//			}

			s_u32PCMOutIdx = s_u32PCMOutIdx + u32PlayByte;
			if(s_u32PCMOutIdx >= s_u32PCMInIdx){
				s_u32PCMOutIdx = 0;
				s_u32PCMInIdx = 0;
			}
		}
		else{
			DEBUGPRINT("DDDDDD AudioPlayThread u32PlayByte == 0 \n");
			s_u32PCMOutIdx = s_u32PCMOutIdx + s_u32PlayFrameSize;
			if(s_u32PCMOutIdx >= s_u32PCMInIdx){
				s_u32PCMOutIdx = 0;
				s_u32PCMInIdx = 0;
			}

		}

		// unlock PCM mutex
		pthread_mutex_unlock(&s_hPCMBufMutex);
		usleep(1000);
	}
	
	return 0;
}

static void ShowUsage()
{
	printf("AECMDemo [options]\n");
	printf("-e enable echo cancel \n");
	printf("-l echo cancel level\n");
	printf("-h help \n");
}

int main(int argc, char **argv)
{
	int i32Err = 0;
	int i32Opt;
	char stdin_ch;
	ERRCODE eErr;
	pthread_t hAudioRecThread;
	pthread_t hAudioPlayThread;
	void *pvThreadRet;
	AecmConfig sAECConfig;

	sAECConfig.cngMode = AecmFalse;
	sAECConfig.echoMode = 4;

	// Parse options
	while ((i32Opt = getopt(argc, argv, "l:eh")) != -1) {
		switch (i32Opt) {
			case 'e': {
				s_bEnEchoCancel = true;
			}
			break;
			case 'l': {
				int32_t i32Level = atoi(optarg);
				
				if((i32Level >= 0) && (i32Level <= 4)){
					sAECConfig.echoMode = i32Level;
					DEBUGPRINT("Set echo cancel level %d\n", i32Level);
				}
			}
			break;
			case 'h': {
				ShowUsage();
				i32Err = 0;
				goto main_done;
			}
			break;
		}
	}

	//Init context
	memset(&s_sAudioRecCtx, 0x00, sizeof(S_NM_AUDIOCTX));
	memset(&s_sAudioPlayCtx, 0x00, sizeof(S_NM_AUDIOCTX));

	s_sAudioRecCtx.u32SampleRate = AUDIO_SR;
	s_sAudioRecCtx.u32ChannelNum = 2;
	s_sAudioRecCtx.ePCMType = eNM_PCM_S16LE;
	
	s_sAudioPlayCtx.u32SampleRate = AUDIO_SR;
	s_sAudioPlayCtx.u32ChannelNum = 2;
	s_sAudioPlayCtx.ePCMType = eNM_PCM_S16LE;

#if !defined(REC_ONLY_TEST)
	// Open audio playback path
	eErr = ALSA_PlayOpen(&s_sAudioPlayCtx, &s_pvAudioPlayRes);
	if(eErr != ERR_ALSA_NONE){
		ERRPRINT("Cannot open play device\n");
		goto main_done;
	}
#endif
	// Open audio record path
	eErr = ALSA_RecOpen(AUDIO_REC_FRAG, &s_sAudioRecCtx, &s_pvAudioRecRes);
	if(eErr != ERR_ALSA_NONE){
		ERRPRINT("Cannot open record device\n");
		goto main_done;
	}


	s_u32RecFrameSize = s_sAudioRecCtx.u32SampleNum * sizeof(uint16_t) * s_sAudioRecCtx.u32ChannelNum;
	s_u32PlayFrameSize = WEBRTC_FAREND_BUF_SAMPLES * sizeof(uint16_t) * s_sAudioPlayCtx.u32ChannelNum;
	
	DEBUGPRINT("s_u32RecFrameSize %d\n", s_u32RecFrameSize);

	//Alloc audio frame buffers
	s_u32PCMBufSize = s_u32RecFrameSize * DEF_PCM_BUF_CNT;
	s_pu8PCMBuf = (uint8_t *)malloc(s_u32PCMBufSize);
	if(s_pu8PCMBuf == NULL){
		ERRPRINT("Cannot allocate PCM buffer \n");
		goto main_done;
	}

	s_u32PCMInIdx = 0;
	s_u32PCMOutIdx = 0;

	// Init WebRTC AECM module
	s_pvAECMInst = WebRtcAecm_Create();
	if(s_pvAECMInst == NULL){
		ERRPRINT("Unable create AECM instance \n");
		i32Err = -1;
		goto main_done;
	}

	i32Err = WebRtcAecm_Init(s_pvAECMInst, AUDIO_SR);
	if(i32Err){
		ERRPRINT("Unable init AECM module \n");
		goto main_done;
	}

	i32Err = WebRtcAecm_set_config(s_pvAECMInst, sAECConfig);
	if(i32Err){
		ERRPRINT("Unable set AECM config \n");
		goto main_done;
	}
	
	//create audio record thread
	i32Err = pthread_create(&hAudioRecThread, NULL, AudioRecThread, NULL);
	if(i32Err != 0){
		ERRPRINT("Cannot create record thread \n");
		goto main_done;
	}

#if !defined(REC_ONLY_TEST)
	//create audio play thread
	i32Err = pthread_create(&hAudioPlayThread, NULL, AudioPlayThread, NULL);
	if(i32Err != 0){
		ERRPRINT("Cannot create play thread \n");
		goto main_done;
	}
#endif

	s_bThreadRun = true;

	while(1){
		stdin_ch = getchar();

		if(stdin_ch == 'q'){
			break;
		}
		else if(stdin_ch == 'e'){
			if(s_bEnEchoCancel == false){
				printf("Enable echo cancel \n");
				s_bEnEchoCancel = true;
			}
			else{
				printf("Disable echo cancel \n");
				s_bEnEchoCancel = false;
			}
		}
	}


main_done:

	if(s_bThreadRun == true){
		s_bThreadRun = false;
		pthread_join(hAudioRecThread, &pvThreadRet);
		pthread_join(hAudioPlayThread, &pvThreadRet);
	}

	if(s_pu8PCMBuf)
		free(s_pu8PCMBuf);

	if(s_pvAudioRecRes)
		ALSA_RecClose(&s_pvAudioRecRes);

	if(s_pvAudioPlayRes)
		ALSA_PlayClose(&s_pvAudioPlayRes);

	if(s_pvAECMInst)
		WebRtcAecm_Free(s_pvAECMInst);

	printf("main done\n");
	return i32Err;
}
