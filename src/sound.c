/*RPCemu v0.6 by Tom Walker
  Sound emulation
  Stripped out of iomd.c, so lots of mess here*/
#include <stdint.h>
#include <allegro.h>
#include "rpcemu.h"
#include "mem.h"
#include "iomd.h"

int getbufferlen(void);

int soundenabled;
uint32_t soundaddr[4];
static int samplefreq;
int soundinited,soundlatch,soundcount;
static unsigned short bigsoundbuffer[8][44100<<1];
static int bigsoundpos=0;
static int bigsoundbufferhead=0; // sound buffer being written to
static int bigsoundbuffertail=0; // sound buffer being read from
static int oldsamplefreq=44100;
static int soundon=0;
static AUDIOSTREAM *as;

#define BUFFERLEN (4410>>1)
//#define BUFFERLEN 11025

//static FILE *sndfile; // used for debugging

void initsound(void)
{
        if (soundon)
        {
                stop_audio_stream(as);
        }
        else
        {
                install_sound(DIGI_AUTODETECT,MIDI_NONE,0);
                samplefreq=44100;
                soundon=1;
        }
        if (soundenabled) as=play_audio_stream(BUFFERLEN,16,1,samplefreq,255,128);
        else              as=play_audio_stream(BUFFERLEN,16,1,samplefreq,0,128);
}

void closesound(void)
{
        stop_audio_stream(as);
        as=play_audio_stream(BUFFERLEN,16,1,samplefreq,0,128);
//        remove_sound();
}

int getbufferlen(void)
{
        int offset=(iomd.sndstat&1)<<1,start,end;
        start=soundaddr[offset]&0xFF0;
        end=(soundaddr[offset+1]&0xFF0)+16;
        return end-start;
}


int getsamplefreq(void)
{
        return samplefreq;
}

void changesamplefreq(int newsamplefreq)
{
        samplefreq = newsamplefreq;
        if (samplefreq!=oldsamplefreq)
        {
        
//                rpclog("Change sample freq from %i to %i\n",oldsamplefreq,samplefreq);
//                stop_audio_stream(as);
//                as=play_audio_stream(BUFFERLEN,16,1,samplefreq,255,128);

                voice_set_frequency(as->voice,samplefreq);
                oldsamplefreq=samplefreq;
        }
}

void stopsound(void)
{
        voice_set_volume(as->voice,0);
}

void continuesound(void)
{
        voice_set_frequency(as->voice,samplefreq);
        voice_set_volume(as->voice,255);
}

void updatesoundirq(void)
{
        uint32_t page,start,end,temp;
        int offset=(iomd.sndstat&1)<<1;
        int len;
        unsigned int c;

        if (!soundenabled)
        {
                return;
        }

        // If bigsoundbufferhead is 1 less than bigsoundbuffertail, then
        // the buffer list is full.
        if (((bigsoundbufferhead+1)&7)==bigsoundbuffertail)
        {
                soundcount+=4000;
                // kick the sound thread to clear the list
                wakeupsoundthread();
                return;
        }
        page=soundaddr[offset]&0xFFFFF000;
        start=soundaddr[offset]&0xFF0;
        end=(soundaddr[offset+1]&0xFF0)+16;
        len=(end-start)>>2;
        soundlatch=(int)((float)((float)len/(float)samplefreq)*2000000.0f);
//        rpclog("soundlatch is %08X %i %03X %i %04X %04X %i\n",soundlatch,soundlatch,len,len,start,end,offset);
                                        iomd.state|=0x10;
                                        updateirqs();
                                        iomd.sndstat|=6;
                                        iomd.sndstat^=1;
        for (c=start;c<end;c+=4)
        {
                temp=ram[((c+page)&rammask)>>2];
                bigsoundbuffer[bigsoundbufferhead][bigsoundpos++]=(temp&0xFFFF);//^0x8000;
                bigsoundbuffer[bigsoundbufferhead][bigsoundpos++]=(temp>>16);//&0x8000;
                if (bigsoundpos>=(BUFFERLEN<<1))
                {
//                        rpclog("Just finished buffer %i\n",bigsoundbufferhead);
                        bigsoundbufferhead++;
                        bigsoundbufferhead&=7;
                        bigsoundpos=0;
                        wakeupsoundthread();
                }
        }
//        fwrite(bigsoundbuffer,len<<2,1,sndfile);
}

int updatesoundbuffer(void)
{
        unsigned short *p;
        int c;

        if (!soundenabled)
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
                        free_audio_stream_buffer(as);
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

