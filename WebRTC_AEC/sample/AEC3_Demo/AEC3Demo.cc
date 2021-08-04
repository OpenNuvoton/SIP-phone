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

#include "api/audio/echo_canceller3_factory.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/high_pass_filter.h"

#include "ALSA.h"

#define ENABLE_HIGH_PASS_FILTER

#define AUDIO_SR 16000
//#define WEBRTC_AEC3_SAMPLES_PER_FRAME (AUDIO_SR / 100)	//used by AEC3 audio buffer operation
#define AUDIO_CHANNEL 2
#define PLAYBACK_CHANNEL	AUDIO_CHANNEL
#define REC_CHANNEL			AUDIO_CHANNEL

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

using namespace webrtc;

static StreamConfig s_sStreamConfig;
static std::unique_ptr<webrtc::EchoControl> echo_controler;

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

	std::unique_ptr<AudioBuffer> RecAudioBuf = std::make_unique<AudioBuffer>(
		AUDIO_SR, REC_CHANNEL,
		AUDIO_SR, REC_CHANNEL,
		AUDIO_SR, REC_CHANNEL);

#if defined (ENABLE_HIGH_PASS_FILTER)
	std::unique_ptr<HighPassFilter> HPFilter = std::make_unique<HighPassFilter>(AUDIO_SR, REC_CHANNEL);
#endif

	pu8TempRecBuf = (uint8_t *)malloc(s_u32RecFrameSize);
	if(pu8TempRecBuf == NULL){
		ERRPRINT("Cannot alloc temp record buffer \n");
		return 0;
	}

	echo_controler->SetAudioBufferDelay(10);

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

			RecAudioBuf->CopyFrom((const int16_t*)s_sAudioRecCtx.pDataVAddr, s_sStreamConfig);

			if (s_bEnEchoCancel){
				u32StartOPTime = GetTimeMicroSec();

#if defined (ENABLE_HIGH_PASS_FILTER)
				HPFilter->Process(RecAudioBuf.get(), true);
#endif
				echo_controler->AnalyzeCapture(RecAudioBuf.get());
				echo_controler->ProcessCapture(RecAudioBuf.get(), false);

				u32EndOPTime = GetTimeMicroSec();

//				DEBUGPRINT("echo cancel spend time is %d us \n", u32EndOPTime - u32StartOPTime);
				RecAudioBuf->CopyTo(s_sStreamConfig, (int16_t*)(s_pu8PCMBuf + s_u32PCMInIdx));
				s_u32PCMInIdx += s_sAudioRecCtx.u32DataSize;				
			}
			else{
//				memcpy((s_pu8PCMBuf + s_u32PCMInIdx), pu8TempRecBuf, s_sAudioRecCtx.u32DataSize);
				RecAudioBuf->CopyTo(s_sStreamConfig, (int16_t*)(s_pu8PCMBuf + s_u32PCMInIdx));
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

	std::unique_ptr<AudioBuffer> PlayAudioBuf = std::make_unique<AudioBuffer>(
		AUDIO_SR, PLAYBACK_CHANNEL,
		AUDIO_SR, PLAYBACK_CHANNEL,
		AUDIO_SR, PLAYBACK_CHANNEL);

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
			PlayAudioBuf->CopyFrom((const int16_t*)s_sAudioPlayCtx.pDataVAddr, s_sStreamConfig);

			if(s_bEnEchoCancel == true){
				echo_controler->AnalyzeRender(PlayAudioBuf.get());

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
	printf("AEC3Demo [options]\n");
	printf("-e enable echo cancel \n");
	printf("-h help \n");
}

int main(int argc, char **argv)
{
	int i32Opt;
	int i32Err = 0;
	char stdin_ch;
	ERRCODE eErr = 0;
	pthread_t hAudioRecThread;
	pthread_t hAudioPlayThread;
	void *pvThreadRet;


	// Parse options
	while ((i32Opt = getopt(argc, argv, "eh")) != -1) {
		switch (i32Opt) {
			case 'e': {
				s_bEnEchoCancel = true;
			}
			break;
			case 'h': {
				ShowUsage();
				return 0;
			}
			break;
		}
	}

	//Init context
	memset(&s_sAudioRecCtx, 0x00, sizeof(S_NM_AUDIOCTX));
	memset(&s_sAudioPlayCtx, 0x00, sizeof(S_NM_AUDIOCTX));

	s_sAudioRecCtx.u32SampleRate = AUDIO_SR;
	s_sAudioRecCtx.u32ChannelNum = REC_CHANNEL;
	s_sAudioRecCtx.ePCMType = eNM_PCM_S16LE;
	
	s_sAudioPlayCtx.u32SampleRate = AUDIO_SR;
	s_sAudioPlayCtx.u32ChannelNum = PLAYBACK_CHANNEL;
	s_sAudioPlayCtx.ePCMType = eNM_PCM_S16LE;

	printf("DDDDDDDDDD main --- 0 \n");
	//Init AEC3
	EchoCanceller3Config aec_config;
	aec_config.filter.export_linear_aec_output = false;
	printf("DDDDDDDDDD main --- 1 \n");
	EchoCanceller3Factory aec_factory = EchoCanceller3Factory(aec_config);
	printf("DDDDDDDDDD main --- 2 \n");
	echo_controler = aec_factory.Create(AUDIO_SR, PLAYBACK_CHANNEL, REC_CHANNEL);

	printf("DDDDDDDDDD main --- 3 \n");

	s_sStreamConfig = StreamConfig(AUDIO_SR, REC_CHANNEL, false);
	printf("DDDDDDDDDD main --- 4 \n");

	// Open audio playback path
	eErr = ALSA_PlayOpen(&s_sAudioPlayCtx, &s_pvAudioPlayRes);
	if(eErr != ERR_ALSA_NONE){
		ERRPRINT("Cannot open play device\n");
		i32Err = eErr;
		goto main_done;
	}
	// Open audio record path
	eErr = ALSA_RecOpen((s_sStreamConfig.num_frames() * 1000) / AUDIO_SR , &s_sAudioRecCtx, &s_pvAudioRecRes);
	if(eErr != ERR_ALSA_NONE){
		ERRPRINT("Cannot open record device\n");
		i32Err = eErr;
		goto main_done;
	}

	s_u32RecFrameSize = s_sAudioRecCtx.u32SampleNum * sizeof(uint16_t) * s_sAudioRecCtx.u32ChannelNum;
	s_u32PlayFrameSize = s_u32RecFrameSize;
	
	DEBUGPRINT("s_u32RecFrameSize %d \n", s_u32RecFrameSize);

	//Alloc audio frame buffers
	s_u32PCMBufSize = s_u32RecFrameSize * DEF_PCM_BUF_CNT;
	s_pu8PCMBuf = (uint8_t *)malloc(s_u32PCMBufSize);
	if(s_pu8PCMBuf == NULL){
		ERRPRINT("Cannot allocate PCM buffer \n");
		goto main_done;
	}

	s_u32PCMInIdx = 0;
	s_u32PCMOutIdx = 0;

	printf("DDDDDDDDDD main --- 5 \n");
	//create audio record thread
	i32Err = pthread_create(&hAudioRecThread, NULL, AudioRecThread, NULL);
	if(i32Err != 0){
		ERRPRINT("Cannot create record thread \n");
		goto main_done;
	}

	printf("DDDDDDDDDD main --- 6 \n");
	//create audio play thread
	i32Err = pthread_create(&hAudioPlayThread, NULL, AudioPlayThread, NULL);
	if(i32Err != 0){
		ERRPRINT("Cannot create play thread \n");
		goto main_done;
	}

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

	printf("main done\n");
	return i32Err;
}
