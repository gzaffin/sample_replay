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

#include <string.h>
#include "kssplay.h"

#ifdef USE_ICONV
#include <iconv.h>
#endif

#include <stdint.h>

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

#include <stdio.h>

#define PCM_DEVICE "default"

#include <string.h>
#include "kssplay.h"

#ifdef USE_ICONV
#include <iconv.h>
#endif

#include <stdint.h>

#endif // #ifdef WIN32

#define CHNL_NUM 1
#define SAMPLE_RATE 44100
#define SAMPLESIZE (sizeof(int16_t))

#ifdef WIN32
#include <mmsystem.h>
#else // #ifdef WIN32
#define MAX_PATH        (PATH_MAX)
#endif // #ifdef WIN32

unsigned int BlocksSent = 0U;
unsigned int BlocksPlayed = 0U;

/* functions prototypes */
#ifdef WIN32
static void WinNT_Check(void);

#else // #ifdef WIN32
static void changemode(bool);
static int _kbhit(void);
static int _getch(void);

#endif // #ifdef WIN32

#ifdef USE_ICONV
static int conv_with_iconv(char *title_orig, char *title_locale, const char *fromcode);

#endif

#if !defined(WIN32)
static void changemode(bool dir)
{
    static struct termios newterm;

    if (termmode != dir)
    {
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

#endif // #if !defined(WIN32)

#ifdef USE_ICONV
/*
// conv_with_iconv
*/

static int conv_with_iconv(char *title_orig, char *title_locale, const char *fromcode)
{
    iconv_t icd = iconv_open("UTF-8", fromcode);

    if (icd != (iconv_t)(-1))
    {
        char *srcstr = title_orig;
        char *deststr = title_locale;

        size_t srclen = (NULL != srcstr) ? strlen(srcstr) + 1 : 0;
        size_t destlen = 1024;
        size_t wrtBytes = 0;

        (void)iconv(icd, NULL, NULL, NULL, NULL); // reset conversion state

        wrtBytes = iconv(icd, &srcstr, &srclen, &deststr, &destlen);
        if (wrtBytes == (size_t)-1)
        {
            /*printf("error iconv\n");*/
            return -1;
        }

        iconv_close(icd);
    }
    else
    {
        /*printf("error iconv_open\n");*/
        return -1;
    }

    return 0;
}

#endif

KSSPLAY *kssplay;
KSS *kss;

const int rate = SAMPLE_RATE, nch = CHNL_NUM, bps = 8 * sizeof(int16_t);
int song_num = 0, play_time = 60, fade_time = 5, loop_num = 1, vol = 0;
int data_length = 0, c;
int quality = 0;
unsigned char PlayingMode = 0U; // Normal Mode

uint8_t PausePlay = 0u;

/* simple-playback.c ... */

/*
 * This example code creates a simple audio stream for playing sound, and
 * generates a sine wave sound effect for it to play as time goes on. This
 * is the simplest way to get up and running with procedural sound.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioStream *stream = NULL;
int current_silence = 0;

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_AudioSpec spec;

    SDL_SetAppMetadata("Example Audio Simple Playback", "1.0", "com.example.audio-simple-playback");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* we don't _need_ a window for audio-only things but it's good policy to have one. */
    /*if (!SDL_CreateWindowAndRenderer("examples/audio/simple-playback", 640, 480, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }*/

    /* We're just playing a single thing here, so we'll use the simplified option.
       We are always going to feed audio in as mono, float32 data at 8000Hz.
       The stream will convert it to whatever the hardware wants on the other side. */
    spec.channels = CHNL_NUM;
    spec.format = SDL_AUDIO_S16;
    spec.freq = SAMPLE_RATE;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    current_silence = SDL_GetSilenceValueForFormat(spec.format);

    /* SDL_OpenAudioDeviceStream starts the device paused. You have to tell it to start! */
    SDL_ResumeAudioStreamDevice(stream);

    printf("Initializing ...\n");

#if !defined(WIN32)
    tcgetattr(STDIN_FILENO, &oldterm);
    changemode(true);

#endif // #if !defined(WIN32)

    printf("[KSSPLAY SAMPLE PROGRAM] q)uit v)ol- V)ol+ p)rev n)ext spacebar for pause\n") ;
#ifdef SAMPLE_RPLY_PRINT
    printf("usedAudioBuffers %u\n",usedAudioBuffers) ;

#endif // #ifdef SAMPLE_RPLY_PRINT

    if (argc < 2) {
#if !defined(WIN32)
        changemode(false);

#endif // #if !defined(WIN32)

        printf("Usage: %s FILENAME\n",argv[0]) ;
        return SDL_APP_FAILURE;
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
        return SDL_APP_FAILURE;
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

    if ((0 == conv_with_iconv(kss->title, data_locale, "SHIFT-JIS")) || (0 == conv_with_iconv(kss->title, data_locale, "CP932")))
    {
#ifdef WIN32
        UINT oldCodePage;
        oldCodePage = GetConsoleOutputCP();
        if (!SetConsoleOutputCP(65001))
        {
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

    KSSPLAY_reset(kssplay, song_num, 0) ;

    KSSPLAY_set_device_quality(kssplay, EDSC_PSG, quality);
    KSSPLAY_set_device_quality(kssplay, EDSC_SCC, quality);
    KSSPLAY_set_device_quality(kssplay, EDSC_OPLL, quality);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    /* see if we need to feed the audio stream more data yet.
       We're being lazy here, but if there's less than half a second queued, generate more.
       A sine wave is unchanging audio--easy to stream--but for video games, you'll want
       to generate significantly _less_ audio ahead of time! */
    size_t minimum_audio = (SAMPLE_RATE * CHNL_NUM * SAMPLESIZE) / 2;  /* Samples per second expressed in bytes. Half of that. */
    if (SDL_GetAudioStreamQueued(stream) < minimum_audio) {
        static int16_t samples[(SAMPLE_RATE * CHNL_NUM) / 2];

        /* generate samples */
        if (PausePlay)
        {
            /*KSSPLAY_calc_silent(kssplay, (SAMPLE_RATE * CHNL_NUM) / 2);*/
            memset(samples, current_silence, sizeof (samples));
        }
        else
        {
            KSSPLAY_calc(kssplay, samples, (SAMPLE_RATE * CHNL_NUM) / 2);
            BlocksSent++;
        }
        BlocksPlayed++;

#ifdef SAMPLE_RPLY_PRINT
        if (0 == (BlocksPlayed % 10))
        {
            printf("+10Blocks\n");
        }
#endif // #ifdef SAMPLE_RPLY_PRINT

        /* feed the new data to the stream. It will queue at the end, and trickle out as the hardware needs more data. */
        SDL_PutAudioStreamData(stream, samples, sizeof (samples));
    }

    /* we're not doing anything with the renderer, so just blank it out. */
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    if (_kbhit()) {
        c = _getch() ;
        switch(c)
        {
            case 'q' :
                printf("\n") ;
#if !defined(WIN32)
                fflush(stdout);
                changemode(false);
#endif // #if !defined(WIN32)

                KSSPLAY_delete(kssplay);
                KSS_delete(kss);

                return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
                break;
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
                PausePlay = (1u == PausePlay) ? 0u : 1u ;

                if (PausePlay)
                {
                    KSSPLAY_set_master_volume(kssplay, -256);
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
                if (tmp_vol < -200) {
                    tmp_vol = -200;
                } else if (tmp_vol > 200) {
                    tmp_vol = 200;
                } else { ; }

                vol = tmp_vol;

                KSSPLAY_set_master_volume(kssplay, vol);
            }
                break ;
        }
    }

    if (0U == (BlocksPlayed % 1U)) {
        unsigned int play_time_ms = 500U * BlocksSent;
        unsigned int play_time_min = play_time_ms / 60000U;
        unsigned int play_time_tmp = play_time_ms - (play_time_min * 60000U);
        unsigned int play_time_sec = play_time_tmp / 1000U;
        unsigned int play_time_msec = play_time_tmp % 1000U;
        printf("$%03d [V%04d] %03d:%02d:%03d\r", song_num, vol, play_time_min, play_time_sec, play_time_msec);
#if !defined(WIN32)
        fflush(stdout);
#endif // #if !defined(WIN32)
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
#if !defined(WIN32)
    changemode(false);
#endif // #if !defined(WIN32)

    KSSPLAY_delete(kssplay);
    KSS_delete(kss);
}

