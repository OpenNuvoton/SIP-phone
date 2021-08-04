//------------------------------------------------------------------
//
// Copyright (c) Nuvoton Technology Corp. All rights reserved.
//
//------------------------------------------------------------------

#ifndef _ALSA_H__
#define _ALSA_H__

#include <stdint.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALSA_VOLUME_MIN	0
#define ALSA_VOLUME_MAX	100

#ifndef ERRCODE
#define ERRCODE int32_t
#endif

#define ALSA_ERR							0x80000000
#define ERR_ALSA_NONE						0
#define ERR_ALSA_NULL_POINTER			(ALSA_ERR | 0x01)
#define ERR_ALSA_MALLOC					(ALSA_ERR | 0x02)
#define ERR_ALSA_CTX					(ALSA_ERR | 0x03)
#define ERR_ALSA_RES					(ALSA_ERR | 0x04)
#define ERR_ALSA_DEV					(ALSA_ERR | 0x04)
#define ERR_ALSA_VOLUME					(ALSA_ERR | 0x04)
#define ERR_ALSA_IO						(ALSA_ERR | 0x04)
#define ERR_ALSA_NOT_READY				(ALSA_ERR | 0x04)


// Boolean type
#if !defined(__cplusplus) && !defined(NM_NBOOL)
	#if defined(bool) && (false != 0 || true != !false)
		#warning bool is redefined: false(0), true(!0)
	#endif

	#undef bool
	#undef false
	#undef true
	#define bool	uint8_t
	#define false	0
	#define true	(!false)
#endif // #if !defined(__cplusplus) && !defined(NM_NBOOL)

// PCM type
typedef enum _E_NM_PCMTYPE {
	eNM_PCM_S16LE	= 0x00,				// Signed 16-bit little-endian
	eNM_PCM_TYPENUM						// Number of PCM type
} E_NM_PCMTYPE;

// ==================================================
// Audio context
// (Define audio source/input or output/record)
// ==================================================
typedef struct _S_NM_AUDIOCTX {
    void			*pDataVAddr;		// Start virtual address of audio data
    void			*pDataPAddr;		// Start physical address of audio data
	uint32_t		u32DataSize;		// Actual byte size of audio data
	uint32_t		u32DataLimit;		// Byte size limit to store audio data
	uint64_t		u64DataTime;		// Millisecond offset of audio data

	E_NM_PCMTYPE	ePCMType;			// PCM type of raw audio data
	uint32_t		u32BitRate;			// bps
	uint32_t		u32ChannelNum;		// > 0
	uint32_t		u32SampleNum;		// Number of sample in context
										// (E.g. 1 sample has 1L + 1R in stereo)
	uint32_t		u32SampleRate;		// Hz

} S_NM_AUDIOCTX;

#define FUN_MUTEX_LOCK(x)		pthread_mutex_lock(x)
#define FUN_MUTEX_UNLOCK(x)		pthread_mutex_unlock(x)

#define Util_Malloc(x)	malloc(x)
#define Util_Free(x,y)	free(x)
#define Util_Calloc(x, y)	calloc(x, y)

uint64_t
Util_GetTime(void);

#if !defined (DEBUGPRINT)
//#undef DEBUG
/* DEBUG MACRO */
#define WHERESTR                                        "[%-20s %-4u {PID:%-4d} %" PRId64 "] "
#define WHEREARG                                        __FUNCTION__, __LINE__, getpid(), Util_GetTime()
#define DEBUGPRINT2(...)                fprintf(stderr, ##__VA_ARGS__)
//#ifdef DEBUG
        #define DEBUGPRINT(_fmt, ...)   DEBUGPRINT2(WHERESTR _fmt, WHEREARG, ##__VA_ARGS__)
//#else
 //       #define DEBUGPRINT(_fmt, ...)
//#endif
#endif


#define ERRPRINT(_fmt, ...)   fprintf(stderr, "[%-20s %-4u {PID:%-4d} %" PRId64 "]" _fmt,  __FUNCTION__, __LINE__, getpid(), Util_GetTime(), ##__VA_ARGS__)


// ==================================================
// API declaration
// ==================================================

// ==================================================
// playback API declaration
// ==================================================

	ERRCODE
	ALSA_PlayOpen(
		S_NM_AUDIOCTX	*psCtx,				// [in] Audio context.
		void			**ppCtxRes			// [out] ALSA context resource
	);


	ERRCODE
	ALSA_PlayClose(
		void			**ppCtxRes			// [out] ALSA context resource
	);

	ERRCODE
	ALSA_PlayWrite(
		S_NM_AUDIOCTX	*psCtx,				// [in] Audio context.
		uint32_t		*pu32WrittenByte,	// [out] Written bytes
		void			*pCtxRes			// [in] ALSA context resource
	);


// ==================================================
// record API declaration
// ==================================================

	ERRCODE
	ALSA_RecOpen(
		uint32_t u32FragmentMS,				// [in] Audio fragment millisecond
		S_NM_AUDIOCTX	*psCtx,				// [in] Audio context.
		void			**ppCtxRes			// [out] ALSA context resource
	);

	ERRCODE
	ALSA_RecClose(
		void			**ppCtxRes			// [out] OSS context resource
	);

	ERRCODE
	ALSA_RecRead(
		S_NM_AUDIOCTX	*psCtx,				// [in] Audio context.
		void			*pCtxRes			// [in] OSS context resource
	);



#ifdef __cplusplus
} // extern "C"
#endif

#endif

