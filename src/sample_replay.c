/* KSSPLAY SAMPLE REPLAY PROGRAM (ISO/IEC 9899 Programming Language C 1999). */
/* Play KSS files. */
/* reference file: vgmplay project's module Stream.c: C Source File for Sound Output */

// define SAMPLE_RPLY_PRINT for additional pieces of information on standard output
//#define SAMPLE_RPLY_PRINT (1)

#ifdef WIN32
#include <stdlib.h>
#include <stdio.h>
#include <conio.h> // it has _kbhit and _getch
#include <windows.h>
#define _Bool BOOL
#define bool _Bool
#define true (1)
#define false (0)

// #include <WinBase.h>
// VOID WINAPI Sleep( _In_ DWORD dwMilliseconds );
// actually I found this
// WINBASEAPI VOID WINAPI Sleep( __in DWORD dwMilliseconds );

#include <string.h>
#include "kssplay.h"

#ifdef USE_ICONV
#include <iconv.h>
#include <string.h>
#endif

#else // #ifdef WIN32

#include <stdlib.h>

#include <stdbool.h>
#include <limits.h> // for PATH_MAX
#include <termios.h>
#include <unistd.h> // for STDIN_FILENO and usleep()
#include <sys/time.h>   // for struct timeval in _kbhit()

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <stdio.h>

#define PCM_DEVICE "default"

#include <string.h>
#include "kssplay.h"

#endif // #ifdef WIN32

#define CHNL_NUM 1
#define SAMPLE_RATE 44100

#ifdef WIN32
#include <mmsystem.h>
#else // #ifdef WIN32
#define MAX_PATH        (PATH_MAX)
#endif // #ifdef WIN32

#ifdef WIN32
#define SAMPLESIZE      (2)
#define BUFSIZE_MAX     (CHNL_NUM * SAMPLE_RATE * SAMPLESIZE) // Maximum Buffer Size in Bytes, i.e. 1 second of 16bits CHNL_NUM * SAMPLE_RATE
#define AUDIOBUFFERS    (200)         // Maximum Buffer Count
//  Windows:    BufferBytesSize = CHNL_NUM * SAMPLE_RATE / 100 * SAMPLESIZE (e.g. 44100 / 100 * 2 = 882 bytes)
//              1 Audio-Buffer = 10 msec, Min: 5
//              Win95- / WinVista-safe: 500 msec
//  Linux:      BufferBytesSize = 1 << BUFSIZELD (1 << 11 = 2048)
//              1 Audio-Buffer = 23.2 msec
//#define BUFSIZELD       (11)          // Buffer Size
unsigned int BufferBytesSize = 0U;  // Buffer Size in Bytes
unsigned short int neededLargeBuffers = 0U;
unsigned short int usedAudioBuffers = AUDIOBUFFERS;     // used AudioBuffers

static volatile bool PauseThread = false; /* in ISO/IEC 9899:1999 <stdbool.h> standard header */
static volatile bool StreamPause = false;
static volatile bool ThreadPauseConfirm = false;
static volatile bool CloseThread = false;

WAVEFORMATEX WaveFmt;
static HWAVEOUT hWaveOut;
static WAVEHDR WaveHdrOut[AUDIOBUFFERS];
static HANDLE hWaveOutThread;
//static DWORD WaveOutCallbackThrID;

static bool WaveOutOpen = false; /* initialized to false */

unsigned int samplesPerBuffer = 0U;
static char BufferOut[AUDIOBUFFERS][BUFSIZE_MAX];

#else // #ifdef WIN32
static char *device = PCM_DEVICE; /* playback device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE; /* sample format */
static unsigned int rate = SAMPLE_RATE; /* stream rate */
static unsigned int channels = CHNL_NUM; /* count of channels */
static int resample = 1; /* enable alsa-lib resampling */
static int period_event = 0; /* produce poll event after each period */

static unsigned int buffer_time = 500000; /* ring buffer length in us */
static unsigned int period_time = 100000; /* period time in us */
static snd_pcm_sframes_t buffer_size; /* frames */
static snd_pcm_sframes_t period_size; /* frames */
static int16_t *samples;
static snd_pcm_sframes_t buff_size;

static snd_pcm_t *handle = NULL;
static snd_pcm_channel_area_t areas = { 0, };
static snd_output_t *output = NULL;

static struct termios oldterm;
static bool termmode = false;

#endif // #ifdef WIN32

KSSPLAY *kssplay;
KSS *kss;

unsigned int BlocksSent = 0U;
unsigned int BlocksPlayed = 0U;

/* functions prototypes */
static unsigned int StartStream(void);
static unsigned int StopStream(void);
#ifdef WIN32
static void PauseStream(bool PauseOn);
static DWORD WINAPI WaveOutThread(void* Arg);
static void BufCheck(void);
static void WinNT_Check(void);
#else // #ifdef WIN32
static void changemode(bool);
static int _kbhit(void);
static int _getch(void);
static void WaveOutLinuxCallBack(bool PausePlay);

static int set_hwparams(snd_pcm_t *handle,
            snd_pcm_hw_params_t *params,
            snd_pcm_access_t access);
static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams);
static int xrun_recovery(snd_pcm_t *handle, int err);
static int write_loop(snd_pcm_t *handle,
              int16_t *samples,
              snd_pcm_channel_area_t *areas);

/*
 *
 */

struct transfer_method {
    snd_pcm_access_t access;
    int (*transfer_loop)(snd_pcm_t *handle,
                 int16_t *samples,
                 snd_pcm_channel_area_t *areas);
};

static struct transfer_method transfer_method = {
    .access = SND_PCM_ACCESS_RW_INTERLEAVED, .transfer_loop = write_loop,
};

#endif // #ifdef WIN32

#ifdef WIN32
static void WinNT_Check(void)
{
    OSVERSIONINFO VerInf;

    VerInf.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&VerInf);
    //WINNT_MODE = (VerInf.dwPlatformId == VER_PLATFORM_WIN32_NT);

    /* Following Systems need larger Audio Buffers:
        - Windows 95 (500+ ms)
        - Windows Vista (200+ ms)
    Tested Systems:
        - Windows 95B
        - Windows 98 SE
        - Windows 2000
        - Windows XP (32-bit)
        - Windows Vista (32-bit)
        - Windows 7 (64-bit)
    */

    neededLargeBuffers = 0U;
    if (VerInf.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
    {
        if (VerInf.dwMajorVersion == 4 && VerInf.dwMinorVersion == 0)
            neededLargeBuffers = 50U;  // Windows 95
    }
    else if (VerInf.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
        if (VerInf.dwMajorVersion == 6 && VerInf.dwMinorVersion == 0)
            neededLargeBuffers = 20U;  // Windows Vista
    }
}
#else // #ifdef WIN32
static void changemode(bool dir)
{
    static struct termios newterm;

    if (termmode == dir)
        return;

    if (dir)
    {
        newterm = oldterm;
        newterm.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newterm);
    }
    else
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldterm);
    }
    termmode = dir;
}

static int _kbhit(void)
{
    struct timeval tv;
    fd_set rdfs;
    int kbret;
    int retval;

    tv.tv_sec = 0;
    tv.tv_usec = 5000;

    FD_ZERO(&rdfs);
    FD_SET(STDIN_FILENO, &rdfs);

    retval = select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
    if (retval == -1)
    {
        kbret = 0;
    }
    else if (retval)
    {
        kbret = FD_ISSET(STDIN_FILENO, &rdfs);
    }
    else
    {
        kbret = 0;
    }

    return kbret;
}

static int _getch(void)
{
    int ch;
    ch = getchar();
    return ch;
}
#endif // #ifdef WIN32

static unsigned int StartStream(void)
{
    unsigned int RetVal = 0xffffffff;
#ifdef WIN32
    UINT16 Cnt;
    HANDLE WaveOutThreadHandle;
    DWORD WaveOutThreadID;
    //char TestStr[0x80];

    if (WaveOutOpen)
        return 0xD0;    // Thread is already active

    // Init Audio
    WaveFmt.wFormatTag = WAVE_FORMAT_PCM;
    WaveFmt.nChannels = CHNL_NUM;
    WaveFmt.nSamplesPerSec = SAMPLE_RATE;
    WaveFmt.wBitsPerSample = (8 * SAMPLESIZE);
    WaveFmt.nBlockAlign = ( WaveFmt.nChannels * WaveFmt.wBitsPerSample ) / 8;
    WaveFmt.nAvgBytesPerSec = WaveFmt.nBlockAlign * WaveFmt.nSamplesPerSec;
    WaveFmt.cbSize = 0;

    BufferBytesSize = CHNL_NUM * SAMPLE_RATE * SAMPLESIZE / 100; /* 1/100 s = 10 ms */
    if (BufferBytesSize > BUFSIZE_MAX)
        BufferBytesSize = BUFSIZE_MAX;

    samplesPerBuffer = BufferBytesSize / SAMPLESIZE / CHNL_NUM;
    if (usedAudioBuffers > AUDIOBUFFERS)
        usedAudioBuffers = AUDIOBUFFERS;

    printf("BufferBytesSize = %u, samplesPerBuffer = %u, usedAudioBuffers = %u\n", BufferBytesSize, samplesPerBuffer, usedAudioBuffers);

    PauseThread = true; /* in ISO/IEC 9899:1999 <stdbool.h> standard header */
    CloseThread = false;

    ThreadPauseConfirm = false;
    StreamPause = false;

    WaveOutThreadHandle = CreateThread(NULL, 0x00, &WaveOutThread, NULL, 0x00,
                                       &WaveOutThreadID);
    if(WaveOutThreadHandle == NULL)
        return 0xC8;        // CreateThread failed
    CloseHandle(WaveOutThreadHandle);

    if((MMRESULT)waveOutOpen(&hWaveOut, WAVE_MAPPER, &WaveFmt, 0x00, 0x00, CALLBACK_NULL) != (MMRESULT)MMSYSERR_NOERROR) /* MMRESULT waveOutOpen(...) */
#else // #ifdef WIN32
    int err;
    snd_pcm_hw_params_t *hwparams = NULL;
    snd_pcm_sw_params_t *swparams = NULL;

    err = snd_output_stdio_attach(&output, stdout, 0);
    if (err < 0) {
        printf("Output failed: %s\n", snd_strerror(err));
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    printf("Playback device is %s\n", device);
    printf("Stream parameters are %uHz, %s, %u channels\n", rate, snd_pcm_format_name(format), channels);

    /* Open the PCM device in playback mode */
    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    /* Allocate parameters object and fill it with default values*/
    snd_pcm_hw_params_alloca(&hwparams);
//    snd_pcm_hw_params_any(handle, hwparams);

    /* Set parameters */
//    if (err = snd_pcm_hw_params_set_access(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
//        printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(err));
//    }

//    if (err = snd_pcm_hw_params_set_format(handle, hwparams, format) < 0) {
//        printf("ERROR: Can't set format. %s\n", snd_strerror(err));
//    }

//    if (err = snd_pcm_hw_params_set_channels(handle, hwparams, channels) < 0) {
//        printf("ERROR: Can't set channels number. %s\n", snd_strerror(err));
//    }

//    if (err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rate, 0) < 0) {
//        printf("ERROR: Can't set rate. %s\n", snd_strerror(err));
//    }

    /* Write parameters */
//    if (err = snd_pcm_hw_params(handle, hwparams) < 0) {
//        printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(err));
//    }

    if ((err = set_hwparams(handle, hwparams, transfer_method.access)) < 0) {
        printf("Setting of hwparams failed: %s\n", snd_strerror(err));
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

#ifdef SAMPLE_RPLY_PRINT
    printf(" buffer_time = %uus\n period_time = %uus\n buffer_size = %ld\n period_size = %ld\n", buffer_time, period_time, buffer_size, period_size);

    /* Resume information */
    printf("PCM name: '%s'\n", snd_pcm_name(handle));

    printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(handle)));

    snd_pcm_hw_params_get_channels(hwparams, &channels);
    printf("channels: %i ", channels);

    if (channels == 1) {
        printf("(mono)\n");
    }
    else if (channels == 2) {
        printf("(stereo)\n");
    }

    snd_pcm_hw_params_get_rate(hwparams, &rate, 0);
    printf("rate: %d bps\n", rate);

    snd_pcm_sw_params_alloca(&swparams);

    if ((err = set_swparams(handle, swparams)) < 0) {
        printf("Setting of swparams failed: %s\n", snd_strerror(err));
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    printf(" buffer_time = %uus\n period_time = %uus\n buffer_size = %ld\n period_size = %ld\n", buffer_time, period_time, buffer_size, period_size);
#endif // #ifdef SAMPLE_RPLY_PRINT

    /* Allocate buffer to hold single period */
    areas.first = 0;
    areas.step = snd_pcm_format_physical_width(format) / 8;
    buff_size = period_size * channels * areas.step /* 2 is sample size when channels = 1 and format = SND_PCM_FORMAT_S16_LE */;
#ifdef SAMPLE_RPLY_PRINT
    printf(" buff_size = %li bytes\n", buff_size);
#endif // #ifdef SAMPLE_RPLY_PRINT
    samples = (int16_t *)malloc(buff_size);
    areas.addr = samples;

//    err = transfer_method.transfer_loop(handle, samples, &areas);
//    if (err < 0) {
//        printf("Transfer failed: %s\n", snd_strerror(err));
//    }
#endif // #ifdef WIN32

#ifdef WIN32
    if (hWaveOut < 0)
    {
        CloseThread = true;
        return 0xC0;        // waveOutOpen failed
    }
    WaveOutOpen = true;

    //sprintf(TestStr, "Buffer 0,0:\t%p\nBuffer 0,1:\t%p\nBuffer 1,0:\t%p\nBuffer 1,1:\t%p\n",
    //      &BufferOut[0][0], &BufferOut[0][1], &BufferOut[1][0], &BufferOut[1][1]);
    //AfxMessageBox(TestStr);
    for (Cnt = 0x00; Cnt < usedAudioBuffers; Cnt ++)
    {
        WaveHdrOut[Cnt].lpData = BufferOut[Cnt];    // &BufferOut[Cnt][0x00];
        WaveHdrOut[Cnt].dwBufferLength = BufferBytesSize;
        WaveHdrOut[Cnt].dwBytesRecorded = 0x00;
        WaveHdrOut[Cnt].dwUser = 0x00;
        WaveHdrOut[Cnt].dwFlags = 0x00;
        WaveHdrOut[Cnt].dwLoops = 0x00;
        WaveHdrOut[Cnt].lpNext = NULL;
        WaveHdrOut[Cnt].reserved = 0x00;
        RetVal = waveOutPrepareHeader(hWaveOut, &WaveHdrOut[Cnt], sizeof(WAVEHDR));
        WaveHdrOut[Cnt].dwFlags |= WHDR_DONE;
    }

    PauseThread = false;

#else // #ifdef WIN32
    /* do something */
#endif // #ifdef WIN32

    RetVal = 0U;

    return RetVal;
}

static unsigned int StopStream(void)
{
    unsigned int RetVal;
#ifdef WIN32
    UINT16 Cnt;

    if (! WaveOutOpen)
        return 0xD8;    // Thread is not active

    CloseThread = true;

    for (Cnt = 0; Cnt < usedAudioBuffers; Cnt ++)
    {
        Sleep(1);
        if (hWaveOutThread == NULL)
            break;
    }

    WaveOutOpen = false;

    /* RetVal = */(void)waveOutReset(hWaveOut);
    for (Cnt = 0x00; Cnt < usedAudioBuffers; Cnt ++)
        /* RetVal = */(void)waveOutUnprepareHeader(hWaveOut, &WaveHdrOut[Cnt], sizeof(WAVEHDR));

    RetVal = waveOutClose(hWaveOut);
    if(RetVal != MMSYSERR_NOERROR)
        return 0xC4;        // waveOutClose failed  -- but why ???
#else // #ifdef WIN32
    free(samples);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
#endif // #ifdef WIN32

    return RetVal;
}

#ifdef WIN32
static void PauseStream(bool PauseOn)
{
    if (! WaveOutOpen)
        return; // Thread is not active

    switch(PauseOn)
    {
    case true:
        waveOutPause(hWaveOut);
        break;
    case false:
        waveOutRestart(hWaveOut);
        break;
    }
    StreamPause = PauseOn;
}

static DWORD WINAPI WaveOutThread(void* Arg)
{
#ifdef NDEBUG
    DWORD RetVal;
#endif // #ifdef NDEBUG

    UINT16 CurBuf;

    UINT32 WrtSmpls;
    //char TestStr[0x80];
    bool DidBuffer = false; // a buffer was processed

    hWaveOutThread = GetCurrentThread();
#ifdef NDEBUG
    RetVal = SetThreadPriority(hWaveOutThread, THREAD_PRIORITY_TIME_CRITICAL);
    if (! RetVal)
    {
        // Error by setting priority
        // try a lower priority, because too low priorities cause sound stuttering
        RetVal = SetThreadPriority(hWaveOutThread, THREAD_PRIORITY_HIGHEST);
    }
#endif // #ifdef NDEBUG

    BlocksSent = 0U;
    BlocksPlayed = 0U;
    while(! CloseThread)
    {
        while(PauseThread && ! CloseThread && ! (StreamPause && DidBuffer))
        {
            ThreadPauseConfirm = true;
            Sleep(1);
        }
        if (CloseThread)
            break;

        BufCheck();
        DidBuffer = false;
        for (CurBuf = 0U; CurBuf < usedAudioBuffers; CurBuf ++)
        {
            if (WaveHdrOut[CurBuf].dwFlags & WHDR_DONE)
            {
                if (WaveHdrOut[CurBuf].dwUser & 0x01)
                    BlocksPlayed++;
                else
                    WaveHdrOut[CurBuf].dwUser |= 0x01;

                KSSPLAY_calc(kssplay, (int16_t *)WaveHdrOut[CurBuf].lpData, (uint32_t)samplesPerBuffer) ;

                WrtSmpls = samplesPerBuffer;

                WaveHdrOut[CurBuf].dwBufferLength = WrtSmpls * SAMPLESIZE;
                waveOutWrite(hWaveOut, &WaveHdrOut[CurBuf], sizeof(WAVEHDR));

                DidBuffer = true;
                BlocksSent++;
                BufCheck();
                //CurBuf = 0x00;
                //break;
            }
            if (CloseThread)
                break;
        }
        Sleep(1);
    }

    hWaveOutThread = NULL;
    return (DWORD)0x00000000;
}

static void BufCheck(void)
{
    UINT16 CurBuf;

    for (CurBuf = 0x00; CurBuf < usedAudioBuffers; CurBuf ++)
    {
        if (WaveHdrOut[CurBuf].dwFlags & WHDR_DONE)
        {
            if (WaveHdrOut[CurBuf].dwUser & 0x01)
            {
                WaveHdrOut[CurBuf].dwUser &= ~0x01;
                BlocksPlayed ++;
            }
        }
    }
}

#else // #ifdef WIN32

static int set_hwparams(snd_pcm_t *handle,
            snd_pcm_hw_params_t *params,
            snd_pcm_access_t access)
{
    unsigned int rrate;
    snd_pcm_uframes_t size;
    int err, dir;

    /* choose all parameters */
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
        return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
    if (err < 0) {
        printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, params, access);
    if (err < 0) {
        printf("Access type not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, params, format);
    if (err < 0) {
        printf("Sample format not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(handle, params, channels);
    if (err < 0) {
        printf("Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
    if (err < 0) {
        printf("Rate %uHz not available for playback: %s\n", rate, snd_strerror(err));
        return err;
    }
    if (rrate != rate) {
        printf("Rate doesn't match (requested %uHz, get %iHz)\n", rate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
    if (err < 0) {
        printf("Unable to set buffer time %u for playback: %s\n", buffer_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &size);
    if (err < 0) {
        printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
        return err;
    }
    buffer_size = size;
    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
    if (err < 0) {
        printf("Unable to set period time %u for playback: %s\n", period_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
    if (err < 0) {
        printf("Unable to get period size for playback: %s\n", snd_strerror(err));
        return err;
    }
    period_size = size;
    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
    int err;

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
        printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (buffer_size / period_size) * period_size);
    if (err < 0) {
        printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_event ? buffer_size : period_size);
    if (err < 0) {
        printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* enable period events when requested */
    if (period_event) {
        err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
        if (err < 0) {
            printf("Unable to set period event: %s\n", snd_strerror(err));
            return err;
        }
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
        printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}

/*
 *   Underrun and suspend recovery
 */

static int xrun_recovery(snd_pcm_t *handle, int err)
{
    printf("stream recovery\n");
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0)
            printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);   /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}

/*
 *   Transfer method - write only
 */

static int write_loop(snd_pcm_t *handle,
              int16_t *samples,
              snd_pcm_channel_area_t *areas)
{
    int16_t *ptr;
    int err, cptr;

    while (1) {
        ptr = samples;
        cptr = period_size;
        while (cptr > 0) {
            err = snd_pcm_writei(handle, ptr, cptr);
            if (err == -EAGAIN) {
                printf("AGAIN.\n");
                fflush(stdout);
                continue;
            }
            else if (err == -EPIPE) {
                printf("XRUN.\n");
                fflush(stdout);
                snd_pcm_prepare(handle);
            }
            else if (err < 0) {
                if (xrun_recovery(handle, err) < 0) {
                    printf("Write error: %s\n", snd_strerror(err));
                    fflush(stdout);
                    //exit(EXIT_FAILURE);
                }
                break;  /* skip one period */
            }
            else { ; }
            ptr += err * channels;
            cptr -= err;
        }
    }
}

void WaveOutLinuxCallBack(bool PausePlay)
{

    if (PausePlay)
    {
        KSSPLAY_calc_silent(kssplay, (uint32_t)period_size);
    }
    else
    {
        KSSPLAY_calc(kssplay, samples, (uint32_t)period_size);
        BlocksSent++;
    }
    BlocksPlayed++;

#ifdef SAMPLE_RPLY_PRINT
    if (0 == (BlocksPlayed % 10))
    {
        printf("+10Blocks\n");
    }
#endif // #ifdef SAMPLE_RPLY_PRINT

}

#endif // ifdef WIN32

int main(int argc, char *argv[]) {
    const int rate = SAMPLE_RATE, nch = CHNL_NUM, bps = 16;
    int song_num = 0, play_time = 60, fade_time = 5, loop_num = 1, vol = 0;
    int data_length = 0, c;
    int quality = 0;
    unsigned char PlayingMode = 0U; // Normal Mode

    bool PausePlay = false;
    /*unsigned int PlayTimeEnd;*/

    printf("Initializing ...\n");

#ifdef WIN32
    WinNT_Check();

    // PlayingMode = 0x00; Normal Mode
    // PlayingMode = 0x01; FM only Mode
    // PlayingMode = 0x02; Normal/FM mixed Mode (NOT in sync, Hardware is a lot faster)
    switch(PlayingMode)
    {
    case 0x00:
        usedAudioBuffers = 10;
        break;
    case 0x01:
        usedAudioBuffers = 0;   // no AudioBuffers needed
        break;
    case 0x02:
        usedAudioBuffers = 5;   // try to sync Hardware/Software Emulator as well as possible
        break;
    }
    if (usedAudioBuffers < neededLargeBuffers) {
        usedAudioBuffers = neededLargeBuffers;
    }
#else // #ifdef WIN32
    tcgetattr(STDIN_FILENO, &oldterm);
    changemode(true);
#endif // #ifdef WIN32

    printf("[KSSPLAY SAMPLE PROGRAM] q)uit v)ol- V)ol+ p)rev n)ext spacebar for pause\n") ;
#ifdef SAMPLE_RPLY_PRINT
    printf("usedAudioBuffers %u\n",usedAudioBuffers) ;
#endif // #ifdef SAMPLE_RPLY_PRINT

    if (argc < 2) {
#if !defined(WIN32)
        changemode(false);
#endif // #if !defined(WIN32)

        printf("Usage: %s FILENAME\n",argv[0]) ;
        exit(-1);
    }

#if defined (IN_MSX_PATH)
#if defined (WIN32)
    KSS_load_kinrou("IN_MSX_PATH/KINROU5.DRV") ;
    KSS_load_opxdrv("IN_MSX_PATH/OPX4KSS.BIN") ;
    KSS_load_fmbios("IN_MSX_PATH/FMPAC.ROM") ;
    KSS_load_mbmdrv("IN_MSX_PATH/MBR143.001") ;
    KSS_load_mpk106("IN_MSX_PATH/MPK.BIN") ;
    KSS_load_mpk103("IN_MSX_PATH/MPK103.BIN") ;
#else // #if defined(WIN32)
    KSS_load_kinrou("IN_MSX_PATH\KINROU5.DRV") ;
    KSS_load_opxdrv("IN_MSX_PATH\OPX4KSS.BIN") ;
    KSS_load_fmbios("IN_MSX_PATH\FMPAC.ROM") ;
    KSS_load_mbmdrv("IN_MSX_PATH\MBR143.001") ;
    KSS_load_mpk106("IN_MSX_PATH\MPK.BIN") ;
    KSS_load_mpk103("IN_MSX_PATH\MPK103.BIN") ;
#endif // #if defined(WIN32)

#endif // #if defined(IN_MSX_PATH)

    if((kss=KSS_load_file(argv[1]))== NULL)
    {
#if !defined(WIN32)
        changemode(false);
#endif // #if !defined(WIN32)

        printf("FILE READ ERROR!\n") ;
        exit(-1) ;
    }

    /* INIT KSSPLAY */
    kssplay = KSSPLAY_new(rate, nch, bps) ;
    KSSPLAY_set_data(kssplay, kss) ;
    vol = kssplay->master_volume ;
#ifdef SAMPLE_RPLY_PRINT
    printf("Vol = %d\n", (vol = kssplay->master_volume));
#endif // #ifdef SAMPLE_RPLY_PRINT

    /* Print title strings */
#ifdef USE_ICONV

    printf("[%s]", kss->idstr);

    char data_locale[1024];
    memset(data_locale, 0, sizeof data_locale);

    iconv_t icd = iconv_open("UTF-8", "SHIFT-JIS");

    if (icd != (iconv_t)(-1))
    {
        char* srcstr = kss->title;
        char* deststr = data_locale;

        size_t srclen = (NULL != srcstr) ? strlen(srcstr) + 1 : 0;
        size_t destlen = 1024;
        size_t wrtBytes = 0;

        iconv(icd, NULL, NULL, NULL, NULL); // reset conversion state

        wrtBytes = iconv(icd, &srcstr, &srclen, &deststr, &destlen);
        if (wrtBytes != (size_t)-1)
        {
#ifdef WIN32
            UINT oldCodePage;
            oldCodePage = GetConsoleOutputCP();
            if (!SetConsoleOutputCP(65001)) {
                printf("error\n");
            }
            fwrite(data_locale, 1, strlen(data_locale) + 1, stdout);
            printf("\n");

            SetConsoleOutputCP(oldCodePage);
#else // WIN32
            printf("%s\n", data_locale);
#endif // WIN32
        }
        else
        {
            printf("%s\n", kss->title);
        }

        if (kss->extra)
        {
            memset(data_locale, 0, sizeof data_locale);
            srcstr = kss->extra;
            deststr = data_locale;

            size_t srclen = (NULL != srcstr) ? strlen(srcstr) + 1 : 0;
            size_t destlen = 1024;
            size_t wrtBytes = 0;

            iconv(icd, NULL, NULL, NULL, NULL); // reset conversion state

            wrtBytes = iconv(icd, &srcstr, &srclen, &deststr, &destlen);
            if (wrtBytes != (size_t)-1)
            {
#ifdef WIN32
                UINT oldCodePage;
                oldCodePage = GetConsoleOutputCP();
                if (!SetConsoleOutputCP(65001)) {
                    printf("error\n");
                }
                fwrite(data_locale, 1, strlen(data_locale) + 1, stdout);
                printf("\n");

                SetConsoleOutputCP(oldCodePage);
#else // WIN32
                printf("%s\n", data_locale);
#endif // WIN32
            }
            else
            {
                printf("%s\n", kss->extra);
            }
        }
        else
        {
            printf("\n");
        }

        iconv_close(icd);
    }
    else
    {
        printf("[%s]", kss->idstr);
        printf("%s\n", kss->title);
        if (kss->extra)
        {
            printf("%s\n", kss->extra);
        }
        else
        {
            printf("\n");
        }
    }

#else // USE_ICONV

    printf("[%s]", kss->idstr);
    printf("%s\n", kss->title);
    if (kss->extra)
    {
        printf("%s\n", kss->extra);
    }
    else
    {
        printf("\n");
    }
#endif // USE_ICONV

    if (0U != StartStream())
    {
#if !defined(WIN32)
        changemode(false);
#endif // #if !defined(WIN32)

        printf("Error opening Sound Device!\n");
        return -1;
    }
    KSSPLAY_reset(kssplay, song_num, 0) ;

    KSSPLAY_set_device_quality(kssplay, EDSC_PSG, quality);
    KSSPLAY_set_device_quality(kssplay, EDSC_SCC, quality);
    KSSPLAY_set_device_quality(kssplay, EDSC_OPLL, quality);

    /*PlayTimeEnd = 0U;*/
#if !defined(WIN32)
    snd_pcm_prepare(handle);

    WaveOutLinuxCallBack(PausePlay);
    (void)snd_pcm_writei(handle, samples, period_size);
//    int err = transfer_method.transfer_loop(handle, samples, &areas);
//    if (err < 0) {
//        printf("Transfer failed: %s\n", snd_strerror(err));
//    }
    WaveOutLinuxCallBack(PausePlay);
    (void)snd_pcm_writei(handle, samples, period_size);
//    int err = transfer_method.transfer_loop(handle, samples, &areas);
//    if (err < 0) {
//        printf("Transfer failed: %s\n", snd_strerror(err));
//    }
    WaveOutLinuxCallBack(PausePlay);
    (void)snd_pcm_writei(handle, samples, period_size);
//    int err = transfer_method.transfer_loop(handle, samples, &areas);
//    if (err < 0) {
//        printf("Transfer failed: %s\n", snd_strerror(err));
//    }
    WaveOutLinuxCallBack(PausePlay);
    (void)snd_pcm_writei(handle, samples, period_size);
//    int err = transfer_method.transfer_loop(handle, samples, &areas);
//    if (err < 0) {
//        printf("Transfer failed: %s\n", snd_strerror(err));
//    }
#endif // #if !defined(WIN32)

    while(1)
    {

#if !defined(WIN32)
        if (snd_pcm_avail_update(handle) >= period_size) {
            WaveOutLinuxCallBack(PausePlay);
            if (PausePlay) {
                memset(samples, 0, period_size * areas.step);
            }
            (void)snd_pcm_writei(handle, samples, period_size);

//            int err = transfer_method.transfer_loop(handle, samples, &areas);
//            if (err < 0) {
//                printf("Transfer failed: %s\n", snd_strerror(err));
//            }
        }
#endif // #if !defined(WIN32)

        if (_kbhit()) {
            c = _getch() ;
            switch(c)
            {
                case 'q' :
                    printf("\n") ;
#if !defined(WIN32)
                    fflush(stdout);
#endif // #if !defined(WIN32)
                    goto quit ;
                case 'n' :
                    song_num = (song_num+1)&0xff ;
                    KSSPLAY_reset(kssplay, song_num, 0) ;
                    BlocksSent = 0U ;
                    break ;
                case 'N' :
                    song_num = (song_num+10)&0xff ;
                    KSSPLAY_reset(kssplay, song_num, 0) ;
                    BlocksSent = 0U ;
                    break ;
                case 'p' :
                    song_num = (song_num+0xff)&0xff ;
                    KSSPLAY_reset(kssplay, song_num, 0) ;
                    BlocksSent = 0U ;
                    break ;
                case 'P' :
                    song_num = (song_num+0xff-9)&0xff ;
                    KSSPLAY_reset(kssplay, song_num, 0) ;
                    BlocksSent = 0U ;
                    break ;
                case ' ' :
                    PausePlay = !PausePlay ;
#ifdef WIN32
                    PauseStream(PausePlay);
#endif // #ifdef WIN32

                    if (PausePlay)
                    {
                        KSSPLAY_set_master_volume(kssplay, -100);
                    }
                    else
                    {
                        KSSPLAY_set_master_volume(kssplay, vol);
                    }
                    break ;
                case 'v' :
                case 'V' :
                {
                    int tmp_vol = vol;
                    if ('v' == c) {
                        tmp_vol -= 10;
                    } else {
                        tmp_vol += 10;
                    }
                    if (tmp_vol < -100) {
                        tmp_vol = -100;
                    } else if (tmp_vol > 0) {
                        tmp_vol = 0;
                    } else { ; }

                    vol = tmp_vol;

                    KSSPLAY_set_master_volume(kssplay, vol);
                }
                    break ;
            }
        }
#if !defined(WIN32)
        if (0U == (BlocksPlayed % 1U)) {
            unsigned int play_time_ms = 100U * BlocksSent;
#else // #if !defined(WIN32)
        if (0U == (BlocksPlayed % 10U)) {
            unsigned int play_time_ms = 10U * BlocksSent;
#endif // #if !defined(WIN32)
            unsigned int play_time_min = play_time_ms / 60000U;
            unsigned int play_time_tmp = play_time_ms - (play_time_min * 60000U);
            unsigned int play_time_sec = play_time_tmp / 1000U;
            unsigned int play_time_msec = play_time_tmp % 1000U;
            printf("$%03d [V%04d] %03d:%02d:%03d\r", song_num, vol, play_time_min, play_time_sec, play_time_msec);
#if !defined(WIN32)
            fflush(stdout);
#endif // #if !defined(WIN32)
        }
    }

quit:
#if !defined(WIN32)
    changemode(false);
#endif // #if !defined(WIN32)

    KSSPLAY_delete(kssplay);
    KSS_delete(kss);

    if (0U != StopStream())
    {
        printf("Error closing Sound Device!\n");
        return -1;
    }

    return 0;
}

