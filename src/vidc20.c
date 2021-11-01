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

/* VIDC20 emulation

 References:
   ARM VIDC20 Datasheet - ARM DDI 0030E
   Acorn Risc PC Technical Reference Manual - ISBN 1 85250 144 8
   VIDC Datasheet - ISBN 1 85250 027 1 (VIDC 1 Datasheet)
   ARM 7500 Datasheet - ARM DDI 0050C
   ARM 7500FE Datasheet - ARM DDI 0077B
   Cirrus Logic CL-PS7500FE Advance Data Book
*/
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rpcemu.h"
#include "cp15.h"
#include "vidc20.h"
#include "keyboard.h"
#include "sound.h"
#include "mem.h"
#include "iomd.h"

static int current_sizex = -1; /**< Width of the video mode, -1 on invalid */
static int current_sizey = -1; /**< Height of the video mode, -1 on invalid */

// Don't resize the window to smaller than this.
static const int MIN_X_SIZE = 320;
static const int MIN_Y_SIZE = 256;

static int doublesize = VIDC_DOUBLE_NONE; /**< Current state of doubling X/Y values */

/* This state is written by the machine thread. The video thread should not read it. */
static struct vidc_state {
        uint32_t palette[256];		/**< Video Palette */
        int palindex;			/**< index into the palette[] array to write to */
        uint32_t border_colour;		/**< Border Colour */
        uint32_t cursor_palette[3];	/**< Cursor Palette */
        uint32_t hdsr,hcsr,hder;
        uint32_t vdsr,vcsr,vcer,vder;
        uint32_t b0,b1;
        uint32_t bit8;
        int palchange;
} vidc;

/* This state is a cached version of the machine state, and is read by the video thread.
   The machine thread should only change them when it has the mutex (and so the video
   thread is not running). */
static struct cached_state {
        struct
        {
                uint32_t r,g,b;
        } pal[256];
	uint32_t *bitmap;
        uint32_t palette[256];		/**< Video Palette */
        uint32_t border_colour;		/**< Border Colour */
        uint32_t cursor_palette[3];	/**< Cursor Palette */
        uint32_t iomd_cinit;
        uint32_t iomd_vidstart;
        uint32_t iomd_vidend;
        uint32_t iomd_vidinit;
        unsigned char iomd_vidcr;
        int vidc_xsize;			/**< X pixel size of VIDC displayed area */
        int vidc_ysize;			/**< Y pixel size of VIDC displayed area */
        int host_xsize;			/**< X pixel size of display including any doublesize doubling */
        int host_ysize;			/**< Y pixel size of display including any doublesize doubling */
        int cursorx;
        int cursory;
        int cursorheight;
        int lastblock;
        int doublesize;
        uint32_t bpp;
        uint8_t *dirtybuffer;
        int threadpending;
} thr;

/* Two dirty buffers, so one can be written to by the main thread
   while the display thread is reading the other */
static uint8_t dirtybuffer1[512 * 4];
static uint8_t dirtybuffer2[512 * 4];

/* Dirty buffer currently in use by main thread */
uint8_t *dirtybuffer = dirtybuffer1;


/**
 * Obtain pointer to given row of image data buffer.
 *
 * @param row Row of image
 * @return Pointer to data
 */
static uint32_t *
video_image_scanline(int row)
{
	return thr.bitmap + (row * current_sizex);
}

/**
 * Prepare and send a video update message to the GUI.
 *
 * thread: video
 *
 * @param yl
 * @param yh
 */
static void
video_update(int yl, int yh)
{
	rpcemu_video_update(thr.bitmap, current_sizex, current_sizey,
	    yl, yh, thr.doublesize, thr.host_xsize, thr.host_ysize);
}

void
initvideo(void)
{
	current_sizex = -1;
	current_sizey = -1;
	memset(&thr, 0, sizeof(thr));
	memset(dirtybuffer1, 0xff, sizeof(dirtybuffer1));
	memset(dirtybuffer2, 0xff, sizeof(dirtybuffer2));
	vidcstartthread();
}

/**
 * Return the width in pixels of the displayed portion of the screen.
 *
 * @return width
 */
int
vidc_get_xsize(void)
{
	return vidc.hder - vidc.hdsr;
}

/**
 * Return the height in pixels of the displayed portion of the screen.
 *
 * @return height
 */
int
vidc_get_ysize(void)
{
	return vidc.vder - vidc.vdsr;
}

/**
 * Return the values of whether the display is being doubled in either
 * direction
 *
 * @param double_x filled in with a bool of X doubling
 * @param double_y filled in with a bool of Y doubling
 */
void
vidc_get_doublesize(int *double_x, int *double_y)
{
	*double_x = doublesize & VIDC_DOUBLE_X;
	*double_y = doublesize & VIDC_DOUBLE_Y;
}

/**
 * Called when emulated screen size changes, resize the display buffer
 *
 * Called when machine thread has video mutex.
 *
 * @param x Width
 * @param y Height
 */
static void
resizedisplay(int x, int y)
{
	if (x < MIN_X_SIZE) {
		x = MIN_X_SIZE;
	}
	if (y < MIN_Y_SIZE) {
		y = MIN_Y_SIZE;
	}

	current_sizex = x;
	current_sizey = y;

	thr.bitmap = realloc(thr.bitmap, x * y * sizeof(uint32_t));

	resetbuffer();
}

void
closevideo(void)
{
	vidcendthread();
}

/**
 * Arrange RGB values for placing directly in QImage.
 *
 * @param r Red value
 * @param g Green value
 * @param b Blue value
 * @return Combined RGB value
 */
static uint32_t
makecol(uint32_t r, uint32_t g, uint32_t b)
{
	return (0xff << 24) | (r << 16) | (g << 8) | b;
}

static void
vidc_palette_update(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		thr.pal[i].r = makecol(vidc.palette[i] & 0xff, 0, 0);
		thr.pal[i].g = makecol(0, (vidc.palette[i] >> 8) & 0xff, 0);
		thr.pal[i].b = makecol(0, 0, (vidc.palette[i] >> 16) & 0xff);

		thr.palette[i] = makecol(vidc.palette[i] & 0xff,
		                         (vidc.palette[i] >> 8) & 0xff,
		                         (vidc.palette[i] >> 16) & 0xff);
	}
	for (i = 0; i < 3; i++) {
		thr.cursor_palette[i] = makecol(vidc.cursor_palette[i] & 0xff,
		                                (vidc.cursor_palette[i] >> 8) & 0xff,
		                                (vidc.cursor_palette[i] >> 16) & 0xff);
	}
	thr.border_colour = makecol(vidc.border_colour & 0xff,
	                            (vidc.border_colour >> 8) & 0xff,
	                            (vidc.border_colour >> 16) & 0xff);
}

/**
 * Called periodically from the machine thread when the refresh timer indicates
 * it is time for a new frame.
 */
void
drawscr(void)
{
	static int lastframeborder = 0;

	// Must get the mutex before altering the thread's state.
	if (!vidctrymutex()) {
		return;
	}

	// If the thread hasn't run since the last request then don't request it again
	if (thr.threadpending) {
		goto unlock_mutex_return;
	}

	thr.vidc_xsize = vidc.hder - vidc.hdsr;
	thr.vidc_ysize = vidc.vder - vidc.vdsr;

	thr.cursorx = vidc.hcsr - vidc.hdsr;
	thr.cursory = vidc.vcsr - vidc.vdsr;
	if (mousehack) {
		mouse_hack_get_pos(&thr.cursorx, &thr.cursory);
	}
	thr.cursorheight = vidc.vcer - vidc.vcsr;

	if (vidc.palchange) {
		vidc_palette_update();
	}

	thr.iomd_cinit = cinit;
	thr.iomd_vidstart = iomd.vidstart;
	thr.iomd_vidend = iomd.vidend;
	thr.iomd_vidinit = iomd.vidinit;
	thr.iomd_vidcr = iomd.vidcr;
	thr.bpp = vidc.bit8;

	if (thr.vidc_xsize < 2) {
		thr.vidc_xsize = 2;
	}
	if (thr.vidc_ysize < 1) {
		thr.vidc_ysize = 480;
	}

	// Have we changed screen size since the last draw?
	if (thr.vidc_xsize != current_sizex || thr.vidc_ysize != current_sizey) {
		resizedisplay(thr.vidc_xsize, thr.vidc_ysize);
	}

	thr.host_xsize = thr.vidc_xsize;
	thr.host_ysize = thr.vidc_ysize;
	thr.doublesize = VIDC_DOUBLE_NONE;

	// Modes below certain sizes are scaled up, e.g. 320x256. Modes with rectangular
	// pixels, e.g. 640x256, are doubled up in the Y direction to look better on
	// square pixel hosts
	if (thr.vidc_xsize <= 448 || (thr.vidc_xsize <= 480 && thr.vidc_ysize <= 352)) {
		thr.host_xsize = thr.vidc_xsize << 1;
		thr.doublesize |= VIDC_DOUBLE_X;
	}
	if (thr.vidc_ysize <= 352) {
		thr.host_ysize = thr.vidc_ysize << 1;
		thr.doublesize |= VIDC_DOUBLE_Y;
	}

	// Store the value of this screen's pixel doubling, used in keyboard.c for mousehack
	doublesize = thr.doublesize;

	// Handle full screen border plotting
	// If not Video cursor DMA enabled or vertical start > vertical end
	if ((thr.iomd_vidcr & 0x20) == 0 || vidc.vdsr > vidc.vder) {
		lastframeborder = 1;
		if (dirtybuffer[0] || vidc.palchange) {
			uint32_t *p;
			int i;

			dirtybuffer[0] = 0;
			vidc.palchange = 0;

			// Fill the bitmap with the border colour
			p = thr.bitmap;
			for (i = 0; i < current_sizex * current_sizey; i++) {
				p[i] = thr.border_colour;
			}

			video_update(0, thr.vidc_ysize);
		}
		goto unlock_mutex_return;
	}

	{
		int x, y;
		int c;
		int lastblock;

		if (vidc.palchange) {
			resetbuffer();
			vidc.palchange = 0;
		}

		if (lastframeborder) {
			lastframeborder = 0;
			resetbuffer();
		}

		x = y = c = 0;
		lastblock = -1;
		while (y < thr.vidc_ysize) {
			static const int xdiff[8] = { 8192, 4096, 2048, 1024, 512, 512, 256, 256 };

			if (dirtybuffer[c++]) {
				lastblock = c;
			}
			x += xdiff[thr.bpp] << 2;
			while (x > thr.vidc_xsize) {
				x -= thr.vidc_xsize;
				y++;
			}
		}
		thr.lastblock = lastblock;
		thr.dirtybuffer = dirtybuffer;
		dirtybuffer = (dirtybuffer == dirtybuffer1) ? dirtybuffer2 : dirtybuffer1;
	}

	{
		const uint32_t invalidate = thr.iomd_vidinit & 0x1f000000;

		// Invalidate Write-TLB entries corresponding to the screen memory,
		// so that writes to this region will cause the dirtybuffer[] to be modified.
		cp15_tlb_invalidate_physical(invalidate);
	}

	thr.threadpending = 1;

	iomd_flyback(0);

	vidcwakeupthread();

unlock_mutex_return:
	// Unlock the mutex before leaving the function
	vidcreleasemutex();
}

/**
 * VIDC display thread. This is called whenever vidcwakeupthread() signals it.
 * It will only be called when it has the vidc mutex.
 *
 * Update bitmap backbuffer with values from hardware video ram, then update screen
 *
 * thread: video
 */
void
vidcthread(void)
{
	const uint32_t vidstart = thr.iomd_vidstart & 0x7ffff0;
	uint32_t vidend;
	int drawit = 0;
	int x, y;
	const uint8_t *ramp;
	uint32_t addr;
	int yl = -1, yh = -1;
	static int oldcursorheight;
	static int oldcursory;

	/* Deal with the possibility of a spurious thread wakeup */
	if (thr.threadpending == 0) {
		return;
	}

	thr.threadpending = 0;

	if (thr.iomd_vidinit & 0x10000000) {
		/* Using DRAM for video */
		/* TODO video could be in DRAM other than simm 0 bank 0 */
		ramp = (const uint8_t *) ram00;
		vidend = (thr.iomd_vidend + 16) & 0x7ffff0;
	} else {
		/* Using VRAM for video */
		ramp = (const uint8_t *) vram;
		vidend = (thr.iomd_vidend + 2048) & 0xfffff0;
		if (vidend > 0x800000) {
			vidend &= 0x7ffff0;
		}
	}

	addr = thr.iomd_vidinit & 0x7fffff;

	drawit = thr.dirtybuffer[addr >> 12];
	if (drawit) {
		yl = 0;
	}

	switch (thr.bpp) {
	case 0: /* 1 bpp on 32 bpp */
		for (y = 0; y < thr.vidc_ysize; y++) {
			uint32_t *vidp = video_image_scanline(y);

			if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
				drawit = 1;
				yh = y + 8;
				if (yl == -1) {
					yl = y;
				}
			}
			if (drawit) {
				yh = y + 1;
			}
			for (x = 0; x < thr.vidc_xsize; x += 8) {
				if (drawit) {
					int xx;
					for (xx = 0; xx < 8; xx += 8) {
#ifdef _RPCEMU_BIG_ENDIAN
						addr ^= 3;
#endif
						vidp[x + xx]     = thr.palette[ramp[addr] & 1];
						vidp[x + xx + 1] = thr.palette[(ramp[addr] >> 1) & 1];
						vidp[x + xx + 2] = thr.palette[(ramp[addr] >> 2) & 1];
						vidp[x + xx + 3] = thr.palette[(ramp[addr] >> 3) & 1];
						vidp[x + xx + 4] = thr.palette[(ramp[addr] >> 4) & 1];
						vidp[x + xx + 5] = thr.palette[(ramp[addr] >> 5) & 1];
						vidp[x + xx + 6] = thr.palette[(ramp[addr] >> 6) & 1];
						vidp[x + xx + 7] = thr.palette[(ramp[addr] >> 7) & 1];
#ifdef _RPCEMU_BIG_ENDIAN
						addr ^= 3;
#endif
						addr++;
					}
				} else {
					addr += 1;
				}
				if (addr == vidend) {
					addr = vidstart;
				}
				if ((addr & 0xfff) == 0) {
					drawit = thr.dirtybuffer[addr >> 12];
					if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
						drawit = 1;
					}
					if (drawit) {
						yh = y + 8;
					}
					if (yl == -1 && drawit) {
						yl = y;
					}
				}
			}
		}
		break;
	case 1: /* 2 bpp on 32 bpp */
		for (y = 0; y < thr.vidc_ysize; y++) {
			uint32_t *vidp = video_image_scanline(y);

			if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
				drawit = 1;
				yh = y + 8;
				if (yl == -1) {
					yl = y;
				}
			}
			if (drawit) {
				yh = y + 1;
			}
			for (x = 0; x < thr.vidc_xsize; x += 4) {
				if (drawit) {
					int xx;
					for (xx = 0; xx < 4; xx += 4) {
#ifdef _RPCEMU_BIG_ENDIAN
						addr ^= 3;
#endif
						vidp[x + xx]     = thr.palette[ramp[addr] & 3];
						vidp[x + xx + 1] = thr.palette[(ramp[addr] >> 2) & 3];
						vidp[x + xx + 2] = thr.palette[(ramp[addr] >> 4) & 3];
						vidp[x + xx + 3] = thr.palette[(ramp[addr] >> 6) & 3];
#ifdef _RPCEMU_BIG_ENDIAN
						addr ^= 3;
#endif
						addr++;
					}
				} else {
					addr += 1;
				}
				if (addr == vidend) {
					addr = vidstart;
				}
				if ((addr & 0xfff) == 0) {
					drawit = thr.dirtybuffer[addr >> 12];
					if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
						drawit = 1;
					}
					if (drawit) {
						yh = y + 8;
					}
					if (yl == -1 && drawit) {
						yl = y;
					}
				}
			}
		}
		break;
	case 2: /* 4 bpp on 32 bpp */
		for (y = 0; y < thr.vidc_ysize; y++) {
			uint32_t *vidp = video_image_scanline(y);

			if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
				drawit = 1;
				yh = y + 8;
				if (yl == -1) {
					yl = y;
				}
			}
			if (drawit) {
				yh = y + 1;
			}
			for (x = 0; x < thr.vidc_xsize; x += 32) {
				if (drawit) {
					int xx;
					for (xx = 0; xx < 32; xx += 8) {
#ifdef _RPCEMU_BIG_ENDIAN
						vidp[x + xx]     = thr.palette[ramp[addr + 3] & 0xf];
						vidp[x + xx + 1] = thr.palette[(ramp[addr + 3] >> 4) & 0xf];
						vidp[x + xx + 2] = thr.palette[ramp[addr + 2] & 0xf];
						vidp[x + xx + 3] = thr.palette[(ramp[addr + 2] >> 4) & 0xf];
						vidp[x + xx + 4] = thr.palette[ramp[addr + 1] & 0xf];
						vidp[x + xx + 5] = thr.palette[(ramp[addr + 1] >> 4) & 0xf];
						vidp[x + xx + 6] = thr.palette[ramp[addr] & 0xf];
						vidp[x + xx + 7] = thr.palette[(ramp[addr] >> 4) & 0xf];
#else
						vidp[x + xx]     = thr.palette[ramp[addr] & 0xf];
						vidp[x + xx + 1] = thr.palette[(ramp[addr] >> 4) & 0xf];
						vidp[x + xx + 2] = thr.palette[ramp[addr + 1] & 0xf];
						vidp[x + xx + 3] = thr.palette[(ramp[addr + 1] >> 4) & 0xf];
						vidp[x + xx + 4] = thr.palette[ramp[addr + 2] & 0xf];
						vidp[x + xx + 5] = thr.palette[(ramp[addr + 2] >> 4) & 0xf];
						vidp[x + xx + 6] = thr.palette[ramp[addr + 3] & 0xf];
						vidp[x + xx + 7] = thr.palette[(ramp[addr + 3] >> 4) & 0xf];
#endif
						addr += 4;
					}
				} else {
					addr += 16;
				}
				if (addr == vidend) {
					addr = vidstart;
				}
				if ((addr & 0xfff) == 0) {
					drawit = thr.dirtybuffer[addr >> 12];
					if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
						drawit = 1;
					}
					if (drawit) {
						yh = y + 8;
					}
					if (yl == -1 && drawit) {
						yl = y;
					}
				}
			}
		}
		break;
	case 3: /* 8 bpp on 32 bpp */
		for (y = 0; y < thr.vidc_ysize; y++) {
			uint32_t *vidp = video_image_scanline(y);

			if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
				drawit = 1;
				yh = y + 8;
				if (yl == -1) {
					yl = y;
				}
			}
			if (drawit) {
				yh = y + 1;
			}
			for (x = 0; x < thr.vidc_xsize; x += 16) {
				if (drawit) {
					int xx;
					for (xx = 0; xx < 16; xx += 4) {
#ifdef _RPCEMU_BIG_ENDIAN
						vidp[x + xx]     = thr.palette[ramp[addr + 3]];
						vidp[x + xx + 1] = thr.palette[ramp[addr + 2]];
						vidp[x + xx + 2] = thr.palette[ramp[addr + 1]];
						vidp[x + xx + 3] = thr.palette[ramp[addr]];
#else
						vidp[x + xx]     = thr.palette[ramp[addr]];
						vidp[x + xx + 1] = thr.palette[ramp[addr + 1]];
						vidp[x + xx + 2] = thr.palette[ramp[addr + 2]];
						vidp[x + xx + 3] = thr.palette[ramp[addr + 3]];
#endif
						addr += 4;
					}
				} else {
					addr += 16;
				}
				if (addr == vidend) {
					addr = vidstart;
				}
				if ((addr & 0xfff) == 0) {
					drawit = thr.dirtybuffer[addr >> 12];
					if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
						drawit = 1;
					}
					if (drawit) {
						yh = y + 8;
					}
					if (yl == -1 && drawit) {
						yl = y;
					}
				}
			}
		}
		break;
	case 4: /* 16 bpp on 32 bpp */
		for (y = 0; y < thr.vidc_ysize; y++) {
			uint32_t *vidp = video_image_scanline(y);

			if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
				drawit = 1;
				yh = y + 8;
				if (yl == -1) {
					yl = y;
				}
			}
			if (drawit) {
				yh = y + 1;
			}
			for (x = 0; x < thr.vidc_xsize; x += 8) {
				if (drawit) {
					int xx;
					for (xx = 0; xx < 8; xx += 2) {
						uint16_t temp16;
						/* VIDC20 format :                      xBBB BBGG GGGR RRRR
						   Windows format : xxxx xxxx RRRR RRRR GGGG GGGG BBBB BBBB */
#ifdef _RPCEMU_BIG_ENDIAN
						temp16 = ramp[addr + 3] | (ramp[addr + 2] << 8);
#else
						temp16 = ramp[addr] | (ramp[addr + 1] << 8);
#endif
						vidp[x + xx] = thr.pal[temp16 & 0xff].r | thr.pal[(temp16 >> 4) & 0xff].g | thr.pal[(temp16 >> 8) & 0xff].b;
#ifdef _RPCEMU_BIG_ENDIAN
						temp16 = ramp[addr + 1] | (ramp[addr] << 8);
#else
						temp16 = ramp[addr + 2] | (ramp[addr + 3] << 8);
#endif
						vidp[x + xx + 1] = thr.pal[temp16 & 0xff].r | thr.pal[(temp16 >> 4) & 0xff].g | thr.pal[(temp16 >> 8) & 0xff].b;
						addr += 4;
					}
				} else {
					addr += 16;
				}
				if (addr == vidend) {
					addr = vidstart;
				}
				if ((addr & 0xfff) == 0) {
					drawit = thr.dirtybuffer[addr >> 12];
					if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
						drawit = 1;
					}
					if (drawit) {
						yh = y + 8;
					}
					if (yl == -1 && drawit) {
						yl = y;
					}
				}
			}
		}
		break;
	case 6: /* 32 bpp on 32 bpp */
		for (y = 0; y < thr.vidc_ysize; y++) {
			uint32_t *vidp = video_image_scanline(y);

			if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
				drawit = 1;
				yh = y + 8;
				if (yl == -1) {
					yl = y;
				}
			}
			if (drawit) {
				yh = y + 1;
			}
			for (x = 0; x < thr.vidc_xsize; x += 4) {
				if (drawit) {
					int xx;
					for (xx = 0; xx < 4; xx++) {
#ifdef _RPCEMU_BIG_ENDIAN
						vidp[x + xx] = thr.pal[ramp[addr + 3]].r | thr.pal[ramp[addr + 2]].g | thr.pal[ramp[addr + 1]].b;
#else
						vidp[x + xx] = thr.pal[ramp[addr]].r | thr.pal[ramp[addr + 1]].g | thr.pal[ramp[addr + 2]].b;
#endif
						addr += 4;
					}
				} else {
					addr += 16;
				}
				if (addr == vidend) {
					addr = vidstart;
				}
				if ((addr & 0xfff) == 0) {
					drawit = thr.dirtybuffer[addr >> 12];
					if (y < (oldcursorheight + oldcursory) && (y >= (oldcursory - 2))) {
						drawit = 1;
					}
					if (drawit) {
						yh = y + 8;
					}
					if (yl == -1 && drawit) {
						yl = y;
					}
				}
			}
		}
		break;
	default:
		fatal("Bad BPP %i\n", thr.bpp);
	}

	/* Cursor layer is plotted over regular display */
	if (thr.cursorheight > 1) {
		/* Calculate host address of cursor data from physical address.
		   This assumes that cursor data is always in DRAM, not VRAM,
		   which is currently true for RISC OS */
		if (thr.iomd_cinit & 0x8000000) {
			ramp = (const uint8_t *) ram1;
		} else if (thr.iomd_cinit & 0x4000000) {
			ramp = (const uint8_t *) ram01;
		} else {
			ramp = (const uint8_t *) ram00;
		}
		addr = thr.iomd_cinit & mem_rammask;
		// printf("Mouse now at %i,%i\n", thr.cursorx, thr.cursory);
		for (y = 0; y < thr.cursorheight; y++) {
			if ((y + thr.cursory) >= thr.vidc_ysize) {
				break;
			}
			if ((y + thr.cursory) >= 0) {
				uint32_t *vidp = video_image_scanline(y + thr.cursory);

				for (x = 0; x < 32; x += 4) {
#ifdef _RPCEMU_BIG_ENDIAN
					addr ^= 3;
#endif
					if ((x + thr.cursorx)     >= 0 && (x + thr.cursorx)     < thr.vidc_xsize && ramp[addr]        & 3) {
						vidp[x + thr.cursorx]     = thr.cursor_palette[(ramp[addr] & 3) - 1];
					}
					if ((x + thr.cursorx + 1) >= 0 && (x + thr.cursorx + 1) < thr.vidc_xsize && (ramp[addr] >> 2) & 3) {
						vidp[x + thr.cursorx + 1] = thr.cursor_palette[((ramp[addr] >> 2) & 3) - 1];
					}
					if ((x + thr.cursorx + 2) >= 0 && (x + thr.cursorx + 2) < thr.vidc_xsize && (ramp[addr] >> 4) & 3) {
						vidp[x + thr.cursorx + 2] = thr.cursor_palette[((ramp[addr] >> 4) & 3) - 1];
					}
					if ((x + thr.cursorx + 3) >= 0 && (x + thr.cursorx + 3) < thr.vidc_xsize && (ramp[addr] >> 6) & 3) {
						vidp[x + thr.cursorx + 3] = thr.cursor_palette[((ramp[addr] >> 6) & 3) - 1];
					}
#ifdef _RPCEMU_BIG_ENDIAN
					addr ^= 3;
#endif
					addr++;
				}
			}
		}

		if (yl > thr.cursory) {
			yl = thr.cursory;
		}
		if (yl == -1) {
			yl = thr.cursory;
		}
		if (thr.cursory < 0) {
			yl = 0;
		}
		if (yh < (thr.cursorheight + thr.cursory)) {
			yh = thr.cursorheight + thr.cursory;
		}
	}
	oldcursorheight = thr.cursorheight;
	oldcursory = thr.cursory;

	/* Clean the dirtybuffer now we have updated eveything in it */
	memset(thr.dirtybuffer, 0, 512 * 4);

	if (yl == -1 || yh == -1) {
		return;
	}

	if (yh > thr.vidc_ysize) {
		yh = thr.vidc_ysize;
	}

	if (yl >= thr.vidc_ysize) {
		yl = thr.vidc_ysize - 1;
	}
	
	/* Copy backbuffer to screen */
	video_update(yl, yh);
}

void
writevidc20(uint32_t val)
{
	int index;
	float freq;

	switch (val >> 28) {
	case 0: /* Video Palette */
		if (val != vidc.palette[vidc.palindex]) {
			vidc.palette[vidc.palindex] = val;
			vidc.palchange = 1;
		}
		/* Increment Video Palette Address, wraparound from 255 to 0 */
		vidc.palindex = (vidc.palindex + 1) & 0xff;
		break;

	case 1: /* Video Palette Address */
		/* These bits should not be set */
		if ((val & 0x0fffff00) != 0) {
			return;
		}
		vidc.palindex = val & 0xff;
		break;

	case 4: /* Border Colour */
		if (val != vidc.border_colour) {
			vidc.border_colour = val;
			vidc.palchange = 1;
		}
		break;

	case 5: /* Cursor Palette Colour 1 */
	case 6: /* Cursor Palette Colour 2 */
	case 7: /* Cursor Palette Colour 3 */
		index = (val >> 28) - 5;
		if (val != vidc.cursor_palette[index]) {
			vidc.cursor_palette[index] = val;
			vidc.palchange = 1;
		}
		break;

	case 8: /* Horizontal Registers */
	case 9: /* Vertical Registers */
		switch (val >> 24) {
		case 0x80: /* Horizontal Cycle Register */
		case 0x81: /* Horizontal Sync Width Register */
		case 0x82: /* Horizontal Border Start Register */
			/* No need to emulate */
			break;

		case 0x83: /* Horizontal Display Start Register */
			vidc.hdsr = val & 0x3ffe;
			break;

		case 0x84: /* Horizontal Display End Register */
			vidc.hder = val & 0x3ffe;
			break;

		case 0x85: /* Horizontal Border End Register */
			/* No need to emulate */
			break;

		case 0x86: /* Horizontal Cursor Start Register */
			if (vidc.hcsr != (val & 0x3fff)) {
				vidc.hcsr = val & 0x3fff;
			}
			break;

		case 0x87: /* Horizontal interlace register */
			/* Program for interlaced display.
			   (VIDC20 only, not ARM7500/FE).
			   No need to emulate */
			break;

		case 0x90: /* Vertical Cycle Register */
		case 0x91: /* Vertical Sync Width Register */
		case 0x92: /* Vertical Border Start Register */
			/* No need to emulate */
			break;

		case 0x93: /* Vertical Display Start Register */
			vidc.vdsr = val & 0x1fff;
			vidc.palchange = 1;
			break;

		case 0x94: /* Vertical Display End Register */
			vidc.vder = val & 0x1fff;
			vidc.palchange = 1;
			break;

		case 0x95: /* Vertical Border End Register */
			/* No need to emulate */
			break;

		case 0x96: /* Vertical Cursor Start Register */
			if (vidc.vcsr != (val & 0x1fff)) {
				vidc.vcsr = val & 0x1fff;
			}
			break;

		case 0x97: /* Vertical Cursor End Register */
			if (vidc.vcer != (val & 0x1fff)) {
				vidc.vcer = val & 0x1fff;
			}
			break;

		default:
			UNIMPLEMENTED("VIDC Horiz/Vert",
			              "Unknown register 0x%08x 0x%02x",
			              val, val >> 24);
		}
		break;

	case 0xa: /* Stereo Image Registers */
		/* VIDC20 only (for 8-bit VIDC10 compatible system).
		   No need to emulate. */
		break;

	case 0xb: /* Sound Registers */
		switch (val >> 24) {
		case 0xb0: /* Sound Frequency Register */
			vidc.b0 = val;
			break;

		case 0xb1: /* Sound Control Register */
			vidc.b1 = val;
			break;

		default: /* Not a valid register - ignore */
			return;
		}

		/* Because either the Sound Frequency or Control register has
		   changed recalculate the sample frequency. */

		/* We only support 16 bit sound (serial sound mode) */
		if(vidc.b1 & 2) {
			val = (vidc.b0 & 0xff) + 2;
			/* Calculated frequency depends on selected clock: */
			if (vidc.b1 & 1) {
				freq = (1000000.0f / (float) val) / 4.0f;
			} else {
				freq = (705600.0f / (float) val) / 4.0f;
			}
			sound_samplefreq_change((int) freq);
		}
		break;

	case 0xc: /* External Register */
		/* Used to control things such as the Sync polarity
		   and the 'external port', no need to emulate. */
		break;

	case 0xd: /* Frequency Synthesizer Register */
		/* Used to set the frequency of the pixel clock.
		   No need to emulate */
		break;

	case 0xe: /* Control Register */
		/* These bits should not be set */
		if ((val & 0x0ff00000) != 0) {
			return;
		}

		if (((val >> 5) & 7) != vidc.bit8) {
			//printf("Change mode - %08X %i\n", val, (val>>5)&7);
			vidc.bit8 = (val >> 5) & 7;
			resetbuffer();
			vidc.palchange = 1;
		}
		break;

	case 0xf: /* Data Control Register */
		/* Used to program the length of the raster, this enables
		   DMA access near the end of the raster line. No need to
		   emulate. */
		break;

	default:
		UNIMPLEMENTED("VIDC register",
		              "Unknown register 0x%08x 0x%x",
		              val, val >> 28);
		break;
	}
}

void
resetbuffer(void)
{
	memset(dirtybuffer, 0xff, 512 * 4);
}
