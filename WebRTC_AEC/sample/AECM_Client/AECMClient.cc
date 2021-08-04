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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "echo_control_mobile.h"
#include "ALSA.h"

#define SERVER "192.168.1.100"
//#define SERVER "127.0.0.1"
#define SERVPORT 3111

#define AUDIO_SR 8000
#define WEBRTC_FAREND_BUF_SAMPLES  80 // number of samples of webrtc farend buffer

#define AUDIO_REC_FRAG ((WEBRTC_FAREND_BUF_SAMPLES * 1000) / AUDIO_SR) //10 millisecond
#define DEF_PCM_BUF_CNT 	6		//Default PCM buffer count.

static bool s_bThreadRun = true;
static bool s_bEnEchoCancel = false;

static S_NM_AUDIOCTX s_sAudioRecCtx;
static S_NM_AUDIOCTX s_sAudioPlayCtx;
static void *s_pvAudioRecRes = NULL;
static void *s_pvAudioPlayRes= NULL;
static uint32_t s_u32RecFrameSize;
static uint32_t s_u32PlayFrameSize;

static uint32_t s_u32RecPCMBufSize;					//Audio record PCM buffer size
static uint8_t *s_pu8RecPCMBuf = NULL;				//Pointer to audio record PCM buffer

static uint32_t s_u32PlayPCMBufSize;				//Audio play PCM buffer size
static uint8_t *s_pu8PlayPCMBuf = NULL;				//Pointer to audio play PCM buffer

static uint32_t s_u32PCMInIdx;					//Audio PCM input index
static uint32_t s_u32PCMOutIdx;					//Audio PCM output index

//static pthread_mutex_t s_hPCMBufMutex;

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

//Audio record thread
static void *AudioRecThread(
	void *argu
)
{
	int32_t i32ClientSD = *((int32_t *)argu);
	uint8_t *pu8TempRecBuf;
	uint32_t u32StartOPTime;
	uint32_t u32EndOPTime;
	int32_t i32WebRTCRet;
	ERRCODE eErr;
	int32_t i32WriteByte = 0;
	
	pu8TempRecBuf = (uint8_t *)malloc(s_u32RecFrameSize);
	if(pu8TempRecBuf == NULL){
		ERRPRINT("Client - Cannot alloc temp record buffer \n");
		return 0;
	}

	while(s_bThreadRun){
		//read data from ADC
		s_sAudioRecCtx.pDataVAddr = pu8TempRecBuf;
		s_sAudioRecCtx.u32DataLimit = s_u32RecFrameSize;

		// Read audio data
		eErr = ALSA_RecRead(&s_sAudioRecCtx, s_pvAudioRecRes);
		if(eErr == ERR_ALSA_NONE){
			if (s_bEnEchoCancel){
				u32StartOPTime = GetTimeMilliSec();

				i32WebRTCRet = WebRtcAecm_Process(s_pvAECMInst,
													(int16_t *)pu8TempRecBuf,
													NULL,
													(int16_t *)s_pu8RecPCMBuf,
													(s_sAudioRecCtx.u32DataSize / sizeof(int16_t) / s_sAudioRecCtx.u32ChannelNum), 
													AUDIO_REC_FRAG);

				u32EndOPTime = GetTimeMilliSec();
//				DEBUGPRINT("echo cancel spend time is %d ms \n", u32EndOPTime - u32StartOPTime);

				if(i32WebRTCRet!=0){
					DEBUGPRINT("WebRtcAecm_Process error %d\n", i32WebRTCRet);
				}

			}
			else{
				memcpy((s_pu8RecPCMBuf), pu8TempRecBuf, s_sAudioRecCtx.u32DataSize);
			}
		}
		else{
			usleep(1000);
			continue;
		}
		
		//write date to internet
		i32WriteByte = write(i32ClientSD, s_pu8RecPCMBuf, s_sAudioRecCtx.u32DataSize);
		if(i32WriteByte < 0){
			continue;
		}
		else if(i32WriteByte == 0){
			DEBUGPRINT("Server had issued a close \n");
			break;
		}
		else{
//			DEBUGPRINT("Send PCM data to client %d bytes \n", i32WriteByte);
		}
		
		usleep(1000);
	}

	return 0;
}

//Audio play thread
static void *AudioPlayThread(
	void *argu
)
{
	int32_t i32ClientSD = *((int32_t *)argu);
	int32_t i32ReadByte = 0;
	uint32_t u32PlayByte;
	int32_t i32WebRTCRet;

	while(s_bThreadRun){
		//read data from internet
		i32ReadByte = read(i32ClientSD, s_pu8PlayPCMBuf, s_u32PlayFrameSize);
		if(i32ReadByte < 0){
			continue;
		}
		else if(i32ReadByte == 0){
			DEBUGPRINT("Server had issued a close \n");
			break;
		}
		else if(i32ReadByte != (int32_t)s_u32PlayFrameSize){
			DEBUGPRINT("Receive PCM data from server %d bytes \n", i32ReadByte);
		}

		//write date to SPU
		s_sAudioPlayCtx.pDataVAddr = s_pu8PlayPCMBuf;
		s_sAudioPlayCtx.u32DataSize = i32ReadByte;
		s_sAudioPlayCtx.u32DataLimit = i32ReadByte;

		u32PlayByte = 0;

		ALSA_PlayWrite(&s_sAudioPlayCtx, &u32PlayByte, s_pvAudioPlayRes);

		if(u32PlayByte){

			if(s_bEnEchoCancel == true){
				i32WebRTCRet = WebRtcAecm_BufferFarend(s_pvAECMInst, (int16_t*)s_sAudioPlayCtx.pDataVAddr, WEBRTC_FAREND_BUF_SAMPLES);

				if(i32WebRTCRet!=0){
					DEBUGPRINT("WebRtcAecm_BufferFarend error %d\n", i32WebRTCRet);
				}
			}

			if(u32PlayByte != s_u32PlayFrameSize){
				DEBUGPRINT("Audio play u32PlayByte!= s_u32PlayFrameSize\n");
			}
		}
		
		usleep(1000);
	}
	
	return 0;
}

static void ShowUsage()
{
	printf("AECMClient [options]\n");
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

	int32_t i32ClientSD;
	char achServerName[255];
	struct sockaddr_in sServerAddr;
	struct hostent *psHost;

	//pthread_mutex_init(&s_hPCMBufMutex, NULL);
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
				return i32Err;
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

	/* The socket() function returns a socket descriptor */
	/* representing an endpoint. The statement also */
	/* identifies that the INET (Internet Protocol) */
	/* address family with the TCP transport (SOCK_STREAM) */
	/* will be used for this socket. */
	/************************************************/
	/* Get a socket descriptor */
	i32ClientSD = socket(AF_INET, SOCK_STREAM, 0);
	if(i32ClientSD < 0){
		perror("Client-socket() error");
		exit(-1);
	}
	else{
		DEBUGPRINT("Client-socket() OK \n");
	}
	
	/* Use the default server name or IP*/
	strcpy(achServerName, SERVER);
	
	memset(&sServerAddr, 0x00, sizeof(struct sockaddr_in));
	sServerAddr.sin_family = AF_INET;
	sServerAddr.sin_port = htons(SERVPORT);
	sServerAddr.sin_addr.s_addr = inet_addr(achServerName);
	
	if(sServerAddr.sin_addr.s_addr == (unsigned long) INADDR_NONE){
		/* When passing the host name of the server as a */
		/* parameter to this program, use the gethostbyname() */
		/* function to retrieve the address of the host server. */
		/***************************************************/
		/* get host address */
		psHost = gethostbyname(achServerName);
		if(psHost == NULL){
			DEBUGPRINT("HOST NOT FOUND -->\n");
			/* h_errno is usually defined in netdb.h */
			DEBUGPRINT("h_errno = %d \n", h_errno);
			close(i32ClientSD);
			exit(-1);
		}
		memcpy(&sServerAddr.sin_addr, psHost->h_addr, sizeof(sServerAddr.sin_addr));
	}

	/* After the socket descriptor is received, the */
	/* connect() function is used to establish a */
	/* connection to the server. */
	/***********************************************/
	/* connect() to server. */
	i32Err = connect(i32ClientSD, (struct sockaddr *)&sServerAddr, sizeof(sServerAddr));
	if(i32Err < 0){
		perror("Client-connect() error");
		close(i32ClientSD);
		exit(-1);
	}
	else{
		DEBUGPRINT("Connect established...\n");
	}

	// Open audio record path
	eErr = ALSA_RecOpen(AUDIO_REC_FRAG, &s_sAudioRecCtx, &s_pvAudioRecRes);
	if(eErr != ERR_ALSA_NONE){
		ERRPRINT("Cannot open record device\n");
		goto main_done;
	}

	// Open audio playback path
	eErr = ALSA_PlayOpen(&s_sAudioPlayCtx, &s_pvAudioPlayRes);
	if(eErr != ERR_ALSA_NONE){
		ERRPRINT("Cannot open play device\n");
		goto main_done;
	}

	s_u32RecFrameSize = s_sAudioRecCtx.u32SampleNum * sizeof(uint16_t) * s_sAudioRecCtx.u32ChannelNum;
	s_u32PlayFrameSize = WEBRTC_FAREND_BUF_SAMPLES * sizeof(uint16_t) * s_sAudioPlayCtx.u32ChannelNum;
	
	DEBUGPRINT("s_u32RecFrameSize %d\n", s_u32RecFrameSize);

	//Alloc audio frame buffers
	s_u32RecPCMBufSize = s_u32RecFrameSize * DEF_PCM_BUF_CNT;
	s_pu8RecPCMBuf = (uint8_t *)malloc(s_u32RecPCMBufSize);
	if(s_pu8RecPCMBuf == NULL){
		ERRPRINT("Cannot allocate PCM buffer \n");
		goto main_done;
	}

	s_u32PlayPCMBufSize = s_u32PlayFrameSize * DEF_PCM_BUF_CNT;
	s_pu8PlayPCMBuf = (uint8_t *)malloc(s_u32PlayPCMBufSize);
	if(s_pu8PlayPCMBuf == NULL){
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
	i32Err = pthread_create(&hAudioRecThread, NULL, AudioRecThread, &i32ClientSD);
	if(i32Err != 0){
		ERRPRINT("Cannot create record thread \n");
		goto main_done;
	}

	//create audio play thread
	i32Err = pthread_create(&hAudioPlayThread, NULL, AudioPlayThread, &i32ClientSD);
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

	close(i32ClientSD);

main_done:

	if(s_bThreadRun == true){
		s_bThreadRun = false;
		pthread_join(hAudioRecThread, &pvThreadRet);
		pthread_join(hAudioPlayThread, &pvThreadRet);
	}

	if(s_pu8RecPCMBuf)
		free(s_pu8RecPCMBuf);

	if(s_pu8PlayPCMBuf)
		free(s_pu8PlayPCMBuf);

	if(s_pvAudioRecRes)
		ALSA_RecClose(&s_pvAudioRecRes);

	if(s_pvAudioPlayRes)
		ALSA_PlayClose(&s_pvAudioPlayRes);

	if(s_pvAECMInst)
		WebRtcAecm_Free(s_pvAECMInst);

	return i32Err;
}
