#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "rpcemu.h"
#include "disc.h"
#include "disc_adf.h"
#include "fdc.h"

static disc_funcs adf_disc_funcs;

static struct {
	FILE *f;
	uint8_t track_data[2][20*1024];

	int dblside;
	int sectors, size, track;
	int dblstep;
	int density;
	int maxsector;
	int skew;
	int write_prot;
} adf[4];

static struct {
	int sector;
	int track;
	int side;
	int drive;

	int readpos;

	int inread;
	int inwrite;
	int inreadaddr;
	int informat;

	int notfound;
	int cur_sector;

	int pause;
	int index;
} adf_state;

void adf_init(void)
{
	memset(adf, 0, sizeof(adf));
	memset(&adf_state, 0, sizeof(adf_state));
}

void adf_load(int drive, const char *fn, int sectors, int size, int sides, int dblstep, int density, int skew)
{
//	rpclog("adf_load: drive=%i fn=%s\n", drive, fn);
	adf[drive].write_prot = 0;
	adf[drive].f = fopen(fn, "rb+");
	if (!adf[drive].f) {
		adf[drive].f = fopen(fn, "rb");
		if (!adf[drive].f) {
			rpclog("adf_load: failed!\n");
			return;
		}
		adf[drive].write_prot = 1;
	}
	fseek(adf[drive].f, -1, SEEK_END);

	adf[drive].sectors = sectors;
	adf[drive].size = size;
	adf[drive].dblside = sides;
	adf[drive].dblstep = dblstep;
	adf[drive].density = density;
	adf[drive].maxsector = (ftell(adf[drive].f)+1 ) / size;
	adf[drive].skew = skew;

	drive_funcs[drive] = &adf_disc_funcs;
	drive_funcs[drive]->seek(drive, disc_get_current_track(drive));
}


static void adf_close(int drive)
{
	if (adf[drive].f)
		fclose(adf[drive].f);
	adf[drive].f = NULL;
}

static void adf_seek(int drive, int track)
{
	if (!adf[drive].f)
		return;
//        rpclog("Seek %i %i\n", drive, track);
	if (adf[drive].dblstep)
		track /= 2;
	adf[drive].track = track;
	if (adf[drive].dblside) {
		fseek(adf[drive].f, track * adf[drive].sectors * adf[drive].size * 2, SEEK_SET);
		fread(adf[drive].track_data[0], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
		fread(adf[drive].track_data[1], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
	} else {
		fseek(adf[drive].f, track * adf[drive].sectors * adf[drive].size, SEEK_SET);
		fread(adf[drive].track_data[0], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
	}
}

static void adf_writeback(int drive, int track)
{
	if (!adf[drive].f)
		return;

	if (adf[drive].dblstep)
		track /= 2;

	if (adf[drive].dblside) {
		fseek(adf[drive].f, track * adf[drive].sectors * adf[drive].size * 2, SEEK_SET);
		fwrite(adf[drive].track_data[0], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
		fwrite(adf[drive].track_data[1], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
	} else {
		fseek(adf[drive].f, track *  adf[drive].sectors * adf[drive].size, SEEK_SET);
		fwrite(adf[drive].track_data[0], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
	}
}

static void adf_readsector(int drive, int sector, int track, int side, int density)
{
	int sector_nr = (sector - adf[drive].skew) + adf[drive].sectors * (track * (adf[drive].dblside ? 2 : 1) + (side ? 1 : 0));

//	rpclog("adf_readsector: drive=%i sector=%i track=%i side=%i density=%i\n", drive, sector, track, side, density);
	adf_state.sector = sector - adf[drive].skew;
	adf_state.track  = track;
	adf_state.side   = side;
	adf_state.drive  = drive;

	if (!adf[drive].f || (side && !adf[drive].dblside) || (density != adf[drive].density) ||
		(track != adf[drive].track) || (sector_nr >= adf[drive].maxsector) ||
		(adf_state.sector > adf[drive].sectors)) {
		adf_state.notfound=500;
		return;
	}

	adf_state.inread  = 1;
	adf_state.readpos = 0;
}

static void adf_writesector(int drive, int sector, int track, int side, int density)
{
	int sector_nr = (sector - adf[drive].skew) + adf[drive].sectors * (track * (adf[drive].dblside ? 2 : 1) + (side ? 1 : 0));

//	rpclog("adf_writesector: drive=%i sector=%i track=%i side=%i density=%i\n", drive, sector, track, side, density);
	adf_state.sector = sector - adf[drive].skew;
	adf_state.track  = track;
	adf_state.side   = side;
	adf_state.drive  = drive;

	if (!adf[drive].f || (side && !adf[drive].dblside) || (density != adf[drive].density) ||
		(track != adf[drive].track) || (sector_nr >= adf[drive].maxsector) ||
		(adf_state.sector > adf[drive].sectors)) {
		adf_state.notfound = 500;
		return;
	}
	adf_state.inwrite = 1;
	adf_state.readpos = 0;
}

static void adf_readaddress(int drive, int track, int side, int density)
{
	if (adf[drive].dblstep)
		track /= 2;

//	rpclog("adf_readaddress: drive=%i track=%i side=%i density=%i\n", drive, track, side, density);
	adf_state.drive = drive;
	adf_state.track = track;
	adf_state.side  = side;

	if (!adf[drive].f || (side && !adf[drive].dblside) ||
		(density != adf[drive].density) || (track != adf[drive].track)) {
		adf_state.notfound=500;
		return;
	}
	adf_state.inreadaddr = 1;
	adf_state.readpos    = 0;
	adf_state.pause = 100;//500;
}

static void adf_format(int drive, int track, int side, int density)
{
	if (adf[drive].dblstep)
		track /= 2;

	adf_state.drive = drive;
	adf_state.track = track;
	adf_state.side  = side;

	if (!adf[drive].f || (side && !adf[drive].dblside) || (density != adf[drive].density) ||
		track != adf[drive].track) {
		adf_state.notfound = 500;
		return;
	}
	adf_state.sector  = 0;
	adf_state.readpos = 0;
	adf_state.informat  = 1;
}

static void adf_stop(void)
{
//	rpclog("adf_stop\n");

	adf_state.pause = 0;
	adf_state.notfound = 0;
	adf_state.inread = 0;
	adf_state.inwrite = 0;
	adf_state.inreadaddr = 0;
	adf_state.informat = 0;
}

static void adf_poll(void)
{
	int c;

	adf_state.index--;
	if (adf_state.index <= 0) {
		adf_state.index = 6250;
		fdc_indexpulse();
	}

	if (adf_state.pause) {
		adf_state.pause--;
		if (adf_state.pause)
			return;
	}

	if (adf_state.notfound) {
		adf_state.notfound--;
		if (!adf_state.notfound)
			fdc_notfound();
	}
	if (adf_state.inread && adf[adf_state.drive].f) {
		fdc_data(adf[adf_state.drive].track_data[adf_state.side][(adf_state.sector * adf[adf_state.drive].size) + adf_state.readpos]);
		adf_state.readpos++;
		if (adf_state.readpos == adf[adf_state.drive].size) {
			adf_state.inread = 0;
			fdc_finishread();
		}
	}
	if (adf_state.inwrite && adf[adf_state.drive].f) {
		if (adf[adf_state.drive].write_prot) {
			fdc_writeprotect();
			adf_state.inwrite = 0;
			return;
		}
		c = fdc_getdata(adf_state.readpos == (adf[adf_state.drive].size - 1));
		if (c == -1)
			return;
		adf[adf_state.drive].track_data[adf_state.side][(adf_state.sector * adf[adf_state.drive].size) + adf_state.readpos] = c;
		adf_state.readpos++;
		if (adf_state.readpos == adf[adf_state.drive].size) {
			adf_state.inwrite = 0;
			fdc_finishread();
			adf_writeback(adf_state.drive, adf_state.track);
		}
	}
	if (adf_state.inreadaddr && adf[adf_state.drive].f) {
		fdc_sectorid(adf_state.track, adf_state.side, adf_state.sector + adf[adf_state.drive].skew, (adf[adf_state.drive].size == 256) ? 1 : ((adf[adf_state.drive].size == 512) ? 2 : 3));
		adf_state.inreadaddr = 0;
		adf_state.sector++;
		if (adf_state.sector >= adf[adf_state.drive].sectors)
			adf_state.sector=0;
	}
	if (adf_state.informat && adf[adf_state.drive].f) {
		if (adf[adf_state.drive].write_prot) {
			fdc_writeprotect();
			adf_state.informat = 0;
			return;
		}
		adf[adf_state.drive].track_data[adf_state.side][(adf_state.sector * adf[adf_state.drive].size) + adf_state.readpos] = 0;
		adf_state.readpos++;
		if (adf_state.readpos == adf[adf_state.drive].size) {
			adf_state.readpos = 0;
			adf_state.sector++;
			if (adf_state.sector == adf[adf_state.drive].sectors) {
				adf_state.informat = 0;
				fdc_finishread();
				adf_writeback(adf_state.drive, adf_state.track);
			}
		}
	}
}

static disc_funcs adf_disc_funcs = {
	.seek        = adf_seek,
	.readsector  = adf_readsector,
	.writesector = adf_writesector,
	.readaddress = adf_readaddress,
	.poll        = adf_poll,
	.format      = adf_format,
	.stop        = adf_stop,
	.close       = adf_close
};
