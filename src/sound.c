/*RPCemu v0.5 by Tom Walker
  Sound emulation
  Stripped out of iomd.c, so lots of mess here*/
#include <allegro.h>
#include "rpcemu.h"

AUDIOSTREAM *as;
#define BUFFERLEN 5000
//#define BUFFERLEN 11025

void initsound()
{
        install_sound(DIGI_AUTODETECT,MIDI_NONE,0);
        as=play_audio_stream(BUFFERLEN,16,1,44100,255,128);
        samplefreq=44100;
}

void closesound()
{
        stop_audio_stream(as);
        remove_sound();
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
unsigned short bigsoundbuffer[2][44100<<1];
int bigsoundbufferselect=0,bigsoundpos=0;
int updatebigsound=0;

int oldsamplefreq=44100;

void changesamplefreq()
{
        if (samplefreq!=oldsamplefreq)
        {
//                rpclog("Change sample freq from %i to %i\n",oldsamplefreq,samplefreq);
                stop_audio_stream(as);
                as=play_audio_stream(BUFFERLEN,16,1,samplefreq,255,128);
                oldsamplefreq=samplefreq;
        }
}

void updatesoundirq()
{
        uint32_t page,start,end,temp;
        int offset=(iomd.sndstat&1)<<1;
        int len;
        int c,d=0;
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
                        bigsoundbufferselect^=1;
                        bigsoundpos=0;
                        soundbufferfull=1;
                }
        }
//        fwrite(bigsoundbuffer,len<<2,1,sndfile);
}
FILE *sndfile;
void updatesoundbuffer()
{
        unsigned short *p;
        int c;
        if (!soundenabled)
        {
                soundbufferfull=0;
                return;
        }
/*        if (!sndfile)
        {
                sndfile=fopen("sound.pcm","wb");
        }*/
        p=get_audio_stream_buffer(as);
        if (!p) return;
        soundbufferfull=0;
//        while (!p)
//              p=get_audio_stream_buffer(as);
        for (c=0;c<(BUFFERLEN<<1);c++)
            p[c]=bigsoundbuffer[bigsoundbufferselect^1][c]^0x8000;
        free_audio_stream_buffer(as);
//                        rpclog("Writing buffer %i\n",bigsoundbufferselect^1);
//        fwrite(bigsoundbuffer[bigsoundbufferselect^1],BUFFERLEN<<2,1,sndfile);
}

