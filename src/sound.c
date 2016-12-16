/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2005-2010 Sarah Walker

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Sound emulation */
#include <assert.h>
#include <stdint.h>
#include <allegro.h>
#include "rpcemu.h"
#include "mem.h"
#include "iomd.h"

uint32_t soundaddr[4];
static int samplefreq = 44100;
int soundinited,soundlatch,soundcount;
static uint16_t bigsoundbuffer[8][44100 << 1]; /**< Temp store, used to buffer
                                                    data between the emulated sound
                                                    and Allegro */
static int bigsoundpos=0;
static int bigsoundbufferhead=0; // sound buffer being written to
static int bigsoundbuffertail=0; // sound buffer being read from
static AUDIOSTREAM *as;

#define BUFFERLEN (4410>>1)

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

/**
 * Called when the VIDC registers controlling
 * sample frequency have changed
 *
 * @param newsamplefreq Sample frequency in Hz (e.g. 44100)
 */
void sound_samplefreq_change(int newsamplefreq)
{
	if (newsamplefreq != samplefreq) {
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

/**
 * Copy data from the emulated sound data into a temp store.
 * Also generates sound interrupts.
 *
 * Called from gentimerirq (iomd.c)
 */
 void sound_irq_update(void)
{
        uint32_t page,start,end,temp;
        int offset = (iomd.sndstat & IOMD_DMA_STATUS_BUFFER) << 1;
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

        iomd.irqdma.status |= IOMD_IRQDMA_SOUND_0;
        updateirqs();

        iomd.sndstat |= (IOMD_DMA_STATUS_INTERRUPT | IOMD_DMA_STATUS_OVERRUN);
        iomd.sndstat ^= IOMD_DMA_STATUS_BUFFER; /* Swap between buffer A and B */

        for (c=start;c<end;c+=4)
        {
                temp = ram00[((c + page) & mem_rammask) >> 2];
                bigsoundbuffer[bigsoundbufferhead][bigsoundpos++]=(temp&0xFFFF);//^0x8000;
                bigsoundbuffer[bigsoundbufferhead][bigsoundpos++]=(temp>>16);//&0x8000;
                if (bigsoundpos>=(BUFFERLEN<<1))
                {
                        bigsoundbufferhead++;
                        bigsoundbufferhead &= 7; /* if (bigsoundbufferhead > 7) { bigsoundbufferhead = 0; } */
                        bigsoundpos=0;
                        sound_thread_wakeup();
                }
        }
}

/**
 * Copy data from the temp store into the Allegro output sound buffer.
 *
 * Called from host platform-specific sound thread function.
 */
void sound_buffer_update(void)
{
        uint16_t *p;
        int c;

        if (!config.soundenabled)
        {
                return;
        }

        while (bigsoundbuffertail!=bigsoundbufferhead)
        {
                p = get_audio_stream_buffer(as);  /* allegro */
                if (p)
                {
                        /* Allegro would like us to fill up a buffer */
                        for (c = 0; c < (BUFFERLEN << 1); c++) {
                                p[c] = bigsoundbuffer[bigsoundbuffertail][c] ^ 0x8000;
                        }
                        /* We have filled the stream's buffer, let allegro know about it */
                        free_audio_stream_buffer(as); /* allegro */

                        bigsoundbuffertail++;
                        bigsoundbuffertail &= 7; /* if (bigsoundbuffertail > 7) { bigsoundbuffertail = 0; } */
                }
                else
                {
                        /* Still playing previous block of data, no need to fill it up yet */
                        break;
                }
        }
}

