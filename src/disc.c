#include <stdio.h>
#include <string.h>
#include "rpcemu.h"
#include "disc.h"
#include "disc_adf.h"
#include "fdc.h"

disc_funcs *drive_funcs[2];

static int disc_drivesel = 0;
static int disc_notfound = 0;
static int current_track[2] = {0, 0};

void
disc_set_drivesel(int drive)
{
	disc_drivesel = drive;
}

int
disc_empty(int drive)
{
	return !drive_funcs[drive];
}

void
disc_poll(void)
{
	if (!disc_empty(disc_drivesel)) {
		if (drive_funcs[disc_drivesel] && drive_funcs[disc_drivesel]->poll)
			drive_funcs[disc_drivesel]->poll();
		if (disc_notfound) {
			disc_notfound--;
			if (!disc_notfound)
				fdc_notfound();
		}
	}
}

int disc_get_current_track(int drive)
{
	return current_track[drive];
}

void disc_seek(int drive, int track)
{
	if (drive_funcs[drive] && drive_funcs[drive]->seek)
		drive_funcs[drive]->seek(drive, track);
//        if (track != oldtrack[drive] && !disc_empty(drive))
//                ioc_discchange_clear(drive);

	current_track[drive] = track;
}

void
disc_readsector(int drive, int sector, int track, int side, int density)
{
	if (drive_funcs[drive] && drive_funcs[drive]->readsector)
		drive_funcs[drive]->readsector(drive, sector, track, side, density);
	else
		disc_notfound = 10000;
}

void
disc_writesector(int drive, int sector, int track, int side, int density)
{
	if (drive_funcs[drive] && drive_funcs[drive]->writesector)
		drive_funcs[drive]->writesector(drive, sector, track, side, density);
	else
		disc_notfound = 10000;
}

void
disc_readaddress(int drive, int track, int side, int density)
{
	if (drive_funcs[drive] && drive_funcs[drive]->readaddress)
		drive_funcs[drive]->readaddress(drive, track, side, density);
	else
		disc_notfound = 10000;
}

void
disc_format(int drive, int track, int side, int density)
{
	if (drive_funcs[drive] && drive_funcs[drive]->format)
		drive_funcs[drive]->format(drive, track, side, density);
	else
		disc_notfound = 10000;
}

void
disc_stop(int drive)
{
	if (drive_funcs[drive] && drive_funcs[drive]->stop)
		drive_funcs[drive]->stop();
}
