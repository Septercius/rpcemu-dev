/*RPCemu v0.6 by Tom Walker
  Sound emulation
  Stripped out of iomd.c, so lots of mess here*/
#include <stdint.h>
#include <allegro.h>
#include "rpcemu.h"
#include "mem.h"
#include "iomd.h"

int soundenabled;
int soundbufferfull;
int getbufferlen();
uint32_t soundaddr[4];
static int samplefreq;
int soundinited,soundlatch,soundcount;

AUDIOSTREAM *as;
#define BUFFERLEN (4410>>1)
//#define BUFFERLEN 11025

int soundon=0;

void initsound()
{
        if (soundon)
        {
                stop_audio_stream(as);
                if (soundenabled) as=play_audio_stream(BUFFERLEN,16,1,44100,0,128);
                else              as=play_audio_stream(BUFFERLEN,16,1,44100,255,128);
        }
        install_sound(DIGI_AUTODETECT,MIDI_NONE,0);
        if (soundenabled) as=play_audio_stream(BUFFERLEN,16,1,44100,0,128);
        else              as=play_audio_stream(BUFFERLEN,16,1,44100,255,128);
        samplefreq=44100;
        soundon=1;
}

void closesound()
{
        stop_audio_stream(as);
        as=play_audio_stream(BUFFERLEN,16,1,44100,0,128);
//        remove_sound();
}

int getbufferlen()
{
        int offset=(iomd.sndstat&1)<<1,start,end;
        start=soundaddr[offset]&0xFF0;
        end=(soundaddr[offset+1]&0xFF0)+16;
        return end-start;
}

int bufferlen=100;
unsigned short sndbuffer[16384];
int sndsamples,sndpos=0,sndoffset=0;

int soundinited;

int soundcount=0,soundlatch;
unsigned short bigsoundbuffer[8][44100<<1];
int bigsoundbufferselect=0,bigsoundpos=0;
int updatebigsound=0;

int oldsamplefreq=44100;

int getsamplefreq()
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

void stopsound()
{
        voice_set_volume(as->voice,0);
}

void continuesound()
{
        voice_set_frequency(as->voice,samplefreq);
        voice_set_volume(as->voice,255);
}

int curbigsoundbuffer=0;

void updatesoundirq()
{
        uint32_t page,start,end,temp;
        int offset=(iomd.sndstat&1)<<1;
        int len;
        unsigned int c;
        if (soundbufferfull && bigsoundbufferselect==curbigsoundbuffer)
        {
                soundcount+=4000;
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
                bigsoundbuffer[bigsoundbufferselect][bigsoundpos++]=(temp&0xFFFF);//^0x8000;
                bigsoundbuffer[bigsoundbufferselect][bigsoundpos++]=(temp>>16);//&0x8000;
                if (bigsoundpos>=(BUFFERLEN<<1))
                {
//                        rpclog("Just finished buffer %i\n",bigsoundbufferselect);
                        bigsoundbufferselect++;
                        bigsoundbufferselect&=7;
                        bigsoundpos=0;
                        soundbufferfull++;
                        wakeupsoundthread();
                }
        }
//        fwrite(bigsoundbuffer,len<<2,1,sndfile);
}

FILE *sndfile;
int updatesoundbuffer()
{
        unsigned short *p;
        int c;
        if (!soundenabled)
        {
                soundbufferfull=0;
                return 0;
        }
/*        if (!sndfile)
        {
                sndfile=fopen("sound.pcm","wb");
        }*/
        p=get_audio_stream_buffer(as);
        while (!p) 
        {
//                sleep(0);
                p=get_audio_stream_buffer(as);
        }
        soundbufferfull--;
//        while (!p)
//              p=get_audio_stream_buffer(as);
        for (c=0;c<(BUFFERLEN<<1);c++)
            p[c]=bigsoundbuffer[curbigsoundbuffer][c]^0x8000;
        free_audio_stream_buffer(as);
//        rpclog("Writing buffer %i\n",curbigsoundbuffer);
        curbigsoundbuffer++;
        curbigsoundbuffer&=7;
        return 0;
//        fwrite(bigsoundbuffer[bigsoundbufferselect^1],BUFFERLEN<<2,1,sndfile);
}

