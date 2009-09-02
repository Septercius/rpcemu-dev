/*RPCemu v0.6 by Tom Walker
  Sound emulation
  Stripped out of iomd.c, so lots of mess here*/
#include <assert.h>
#include <stdint.h>
#include <allegro.h>
#include "rpcemu.h"
#include "mem.h"
#include "iomd.h"

int getbufferlen(void);

uint32_t soundaddr[4];
static int samplefreq = 44100;
int soundinited,soundlatch,soundcount;
static unsigned short bigsoundbuffer[8][44100<<1];
static int bigsoundpos=0;
static int bigsoundbufferhead=0; // sound buffer being written to
static int bigsoundbuffertail=0; // sound buffer being read from
static AUDIOSTREAM *as;

#define BUFFERLEN (4410>>1)
//#define BUFFERLEN 11025

//static FILE *sndfile; // used for debugging

/**
 * Called on program startup to initialise the sound system
 */
void sound_init(void)
{
        /* Call the platform code to create a thread for handing sound updates */
        sound_thread_start();

        install_sound(DIGI_AUTODETECT, MIDI_NONE, 0); /* allegro */
        samplefreq = 44100;

        if (config.soundenabled) {
                as = play_audio_stream(BUFFERLEN, 16, 1, samplefreq, 255, 128); /* allegro */
        } else {
                as = play_audio_stream(BUFFERLEN, 16, 1, samplefreq, 0, 128); /* allegro */
        }
}

/**
 * Called when the user turns the sound on via the GUI
 */
void sound_restart(void)
{
        assert(config.soundenabled);

        /* Stop the previously playing 'silent' stream */
        stop_audio_stream(as); /* allegro */

        /* Play an audible stream */
        as = play_audio_stream(BUFFERLEN, 16, 1, samplefreq, 255, 128); /* allegro */
}

/**
 * Called when the user turns the sound off via the GUI
 */
void sound_pause(void)
{
	assert(!config.soundenabled);

        /* Stop the previously playing audible stream */
        stop_audio_stream(as); /* allegro */

        /* Play a zero volume 'silent' stream */
        as = play_audio_stream(BUFFERLEN, 16, 1, samplefreq, 0, 128); /* allegro */
}

void changesamplefreq(int newsamplefreq)
{
	if (newsamplefreq != samplefreq) {
		// rpclog("Change sample freq from %i to %i\n", samplefreq, newsamplefreq);
		samplefreq = newsamplefreq;

		voice_set_frequency(as->voice, samplefreq); /* allegro */
	}
}

/**
 * Silence the sound stream. Used by the GUI to prevent the
 * contents of the sound buffer being played repeatedly whilst
 * the user is altering configuration and the emulation is paused
 */
void sound_mute(void)
{
        voice_set_volume(as->voice, 0); /* allegro */
}

/**
 * Restore the volume of the sound stream.
 */
void sound_unmute(void)
{
        voice_set_frequency(as->voice, samplefreq); /* allegro */
        voice_set_volume(as->voice, 255);  /* allegro */
}

void updatesoundirq(void)
{
        uint32_t page,start,end,temp;
        int offset=(iomd.sndstat&1)<<1;
        int len;
        unsigned int c;

        if (!config.soundenabled)
        {
                return;
        }

        // If bigsoundbufferhead is 1 less than bigsoundbuffertail, then
        // the buffer list is full.
        if (((bigsoundbufferhead+1)&7)==bigsoundbuffertail)
        {
                soundcount+=4000;
                // kick the sound thread to clear the list
                sound_thread_wakeup();
                return;
        }
        page=soundaddr[offset]&0xFFFFF000;
        start=soundaddr[offset]&0xFF0;
        end=(soundaddr[offset+1]&0xFF0)+16;
        len=(end-start)>>2;
        soundlatch=(int)((float)((float)len/(float)samplefreq)*2000000.0f);
//        rpclog("soundlatch is %08X %i %03X %i %04X %04X %i\n",soundlatch,soundlatch,len,len,start,end,offset);
                                        iomd.irqdma.status |= IOMD_IRQDMA_SOUND_0;
                                        updateirqs();
                                        iomd.sndstat|=6;
                                        iomd.sndstat^=1;
        for (c=start;c<end;c+=4)
        {
                temp = ram[((c + page) & config.rammask) >> 2];
                bigsoundbuffer[bigsoundbufferhead][bigsoundpos++]=(temp&0xFFFF);//^0x8000;
                bigsoundbuffer[bigsoundbufferhead][bigsoundpos++]=(temp>>16);//&0x8000;
                if (bigsoundpos>=(BUFFERLEN<<1))
                {
//                        rpclog("Just finished buffer %i\n",bigsoundbufferhead);
                        bigsoundbufferhead++;
                        bigsoundbufferhead&=7;
                        bigsoundpos=0;
                        sound_thread_wakeup();
                }
        }
//        fwrite(bigsoundbuffer,len<<2,1,sndfile);
}

int updatesoundbuffer(void)
{
        unsigned short *p;
        int c;

        if (!config.soundenabled)
        {
                return 0;
        }
/*        if (!sndfile)
        {
                sndfile=fopen("sound.pcm","wb");
        }*/
        while (bigsoundbuffertail!=bigsoundbufferhead)
        {
                p=get_audio_stream_buffer(as);
                if (p)
                {
                        for (c=0;c<(BUFFERLEN<<1);c++)
                                p[c]=bigsoundbuffer[bigsoundbuffertail][c]^0x8000;
                        free_audio_stream_buffer(as); /* allegro */
//                        rpclog("Writing buffer %i\n",bigsoundbuffertail);
//                      fwrite(bigsoundbuffer[bigsoundbufferhead^1],BUFFERLEN<<2,1,sndfile);
                        bigsoundbuffertail++;
                        bigsoundbuffertail&=7;
                }
                else
                {
                        // No free audio buffer, try again later.
                        break;
                }
        }
        return 0;
}

