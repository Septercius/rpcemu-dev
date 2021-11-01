typedef struct {
	void (*seek)(int drive, int track);
	void (*readsector)(int drive, int sector, int track, int side, int density);
	void (*writesector)(int drive, int sector, int track, int side, int density);
	void (*readaddress)(int drive, int track, int side, int density);
	void (*format)(int drive, int track, int side, int density);
	void (*stop)();
	void (*poll)();
	void (*close)(int drive);
} disc_funcs;

extern disc_funcs *drive_funcs[2];

extern void disc_set_drivesel(int drive);
extern void disc_poll(void);
extern int disc_get_current_track(int drive);
extern void disc_seek(int drive, int track);
extern void disc_readsector(int drive, int sector, int track, int side, int density);
extern void disc_writesector(int drive, int sector, int track, int side, int density);
extern void disc_readaddress(int drive, int track, int side, int density);
extern void disc_format(int drive, int track, int side, int density);
extern void disc_stop(int drive);
