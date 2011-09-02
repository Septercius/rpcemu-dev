/* I2C + CMOS RAM emulation

  I2C and Philips PCF8583.

  Includes code from Softgun by Jochen Karrer used under the terms of the
  GPLv2.
*/
#include <stdint.h>
#include <allegro.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "rpcemu.h"
#include "cmos.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr, x); }
#else
#define dbgprintf(x...)
#endif

int i2cclock = 1; /**< The current value of the I2C clock pin */
int i2cdata  = 1; /**< The current value of the I2C data pin */

#define BIN2BCD(val)	((((val) / 10) << 4) | ((val) % 10))

static unsigned char cmosram[256];

/****************************************************************************/

/* returnvalues for the start and write I2C Operations */
#define I2C_ACK		0
#define I2C_NACK	1
#define I2C_DONE	3

#define I2C_READ	5
#define I2C_WRITE	6

/** Struct to hold the function callbacks for operations on a class
    of I2C connected device */
typedef struct {
	int (*start)(void *dev, int i2c_addr, int operation);
	void (*stop)(void *dev);
	int (*write)(void *dev, uint8_t data);
	int (*read)(void *dev, uint8_t *data);
	/*
	 * -----------------------------------------------------
	 * Tell device if read was acked or not
	 * The device can then decide to prepare next
	 * or trigger some finishing action
	 * -----------------------------------------------------
	 */
	void (*read_ack)(void *dev, int ack);
} I2C_SlaveOps;

/** Struct to hold the representation of a class of I2C connected device */
typedef struct {
	const I2C_SlaveOps *devops;
	void *dev;
	uint8_t address;
} I2C_Slave;

/****************************************************************************/

#define PCF_STATE_ADDR	0
#define PCF_STATE_DATA	2

/** The state of the Philips RTC chip */
typedef struct {
	I2C_Slave i2c_slave;
	uint16_t reg_address;
	int state;
} PCF8583;

static void
cmos_update_settings(void)
{
	time_t now = time(NULL);
	const struct tm *t = gmtime(&now);

	/* The year should be stored too, otherwise RISC OS refuses to
	 * read any time from the CMOS/RTC chip!
	 */
	/* The standard C time functionality subtracts 1900 from the year */
	cmosram[0xc0] = (t->tm_year + 1900) % 100;
	cmosram[0xc1] = (t->tm_year + 1900) / 100;

	/* Automatically configure the mousetype depending on which machine
	   model is selected. CMOS location has been verified on 3.50-Select 4
	   and 5.17 (*configure mousetype <number>) */
	if (config.model == CPUModel_ARM7500 ||
	    config.model == CPUModel_ARM7500FE)
	{
		cmosram[0x5d] = 3; /* PS/2 mouse */
	} else {
		cmosram[0x5d] = 0; /* Quadrature mouse */
	}

	// What about also initialising some parts to sensible defaults?
	// eg default bootfs, number of IDE discs, floppy etc....
}

static void
cmos_update_checksum(void)
{
	unsigned checksum = 0;
	int i;

	/* Loop through all but one address of the NVRAM. The checksum will go
	   at the last address */
	for (i = 0; i < 239; i++) {
		/* RISC OS uses addresses that are offset by 0x40 */
		int offset = i + 0x40;

		/* Wrap round the address, skipping addresses 0 .. 15 which
		   are time and config registers */
		if (offset > 255) {
			offset -= 240;
		}

		checksum += (unsigned) cmosram[offset];
	}

	/* Checksum goes at last address (offset by 0x40 for RISC OS) */
	cmosram[0x3f] = (checksum + 1) & 0xff;
}

void loadcmos(void)
{
        char fn[512];
        FILE *cmosf;

        /* Append "cmos.ram" to the given executable path */
        append_filename(fn, rpcemu_get_datadir(), "cmos.ram", sizeof(fn) - 1);
        cmosf = fopen(fn, "rb");

        if (cmosf) {
                /* File open suceeded, load CMOS data */
                if (fread(cmosram, 1, 256, cmosf) != 256) {
                        fatal("Unable to read from CMOS file '%s', %s", fn,
                              strerror(errno));
                }
                fclose(cmosf);
        } else {
                /* Report failure and initialise the array */
                fprintf(stderr, "Could not open CMOS file '%s': %s\n", fn, 
                        strerror(errno));
                memset(cmosram, 0, 256);
        }

	/* Dynamically update CMOS settings */
	cmos_update_settings();

        /* Update the checksum used by RISC OS, as updating values above will
           probably have invalidated it */
        cmos_update_checksum();

        /* Clear the bytes that correspond to registers (i.e. not NVRAM) */
        memset(cmosram, 0, 16);
}

void savecmos(void)
{
        char fn[512];
        FILE *cmosf;

        append_filename(fn, rpcemu_get_datadir(), "cmos.ram", sizeof(fn) - 1);
        cmosf = fopen(fn, "wb");

        if (cmosf) {
                if(fwrite(cmosram, 256, 1, cmosf) != 1) {
                        fatal("Unable to write CMOS file '%s': %s", fn,
                              strerror(errno));
                        // TODO does it have to be fatal?
                }
                fclose(cmosf);
        } else {
                fprintf(stderr, "Could not open CMOS file '%s' for writing: %s\n", fn,
                        strerror(errno));
        }
}

static void cmosgettime(void)
{
	time_t now = time(NULL);
	const struct tm *t = gmtime(&now);

	cmosram[1] = 0;
	cmosram[2] = BIN2BCD(t->tm_sec);
	cmosram[3] = BIN2BCD(t->tm_min);
	cmosram[4] = BIN2BCD(t->tm_hour);
	cmosram[5] = (((t->tm_year + 1900) & 3) << 6) | BIN2BCD(t->tm_mday);
	cmosram[6] = (t->tm_wday << 5) | BIN2BCD(t->tm_mon + 1);
}

/**
 * Write to the PCF8583. Handles write state-machine logic.
 *
 * @param dev  Pointer to the PCF8583 data block
 * @param data Byte being written (address or data)
 * @return I2C state-machine return code
 */
static int
pcf8583_write(void *dev, uint8_t data)
{
	PCF8583 *pcf = dev;

	if (pcf->state == PCF_STATE_ADDR) {
		dbgprintf("PCF8583 Addr 0x%02x\n", data);
		pcf->reg_address = data;
		pcf->state = PCF_STATE_DATA;
	} else if (pcf->state == PCF_STATE_DATA) {
		dbgprintf("PCF8583 Write 0x%02x to %04x\n",
		          data, pcf->reg_address);

		cmosram[pcf->reg_address] = data;

		pcf->reg_address = (pcf->reg_address + 1) & 0xff;
	}
	return I2C_ACK;
}

/**
 * Read from the PCF8583. Will use address previously written to the chip.
 *
 * @param dev  Pointer to the PCF8583 data block
 * @param data Filled in with the value at previously specified address
 * @return I2C state-machine return code
 */
static int
pcf8583_read(void *dev, uint8_t *data)
{
	PCF8583 *pcf = dev;

	if (pcf->reg_address < 0x10)
		cmosgettime();

	*data = cmosram[pcf->reg_address];

	dbgprintf("PCF8583 read 0x%02x from %04x\n", *data, pcf->reg_address);
	pcf->reg_address = (pcf->reg_address + 1) & 0xff;
	return I2C_DONE;
}

/**
 * Initialise the state of the Philips PCF8583 RTC chip.
 * Called when I2C first calls the chip
 *
 * @param dev       Pointer to the PCF8583 data block
 * @param i2c_addr  UNUSED
 * @param operation UNUSED
 * @return I2C state machine return code
 */
static int
pcf8583_start(void *dev, int i2c_addr, int operation)
{
	PCF8583 *pcf = dev;

	dbgprintf("pcf8583 start\n");
	pcf->state = PCF_STATE_ADDR;

	return I2C_ACK;
}

/**
 * Finalise the state of the Philips RTC chip.
 * Called when I2C thinks there's no more to this transaction.
 *
 * @param dev Pointer to the PCF8583 data block
 */
static void
pcf8583_stop(void *dev)
{
	PCF8583 *pcf = dev;

	dbgprintf("pcf8583 stop\n");

	pcf->state = PCF_STATE_ADDR;
}

/** Function pointers for the Philips RTC interaction */
static const I2C_SlaveOps pcf8583_ops = {
	pcf8583_start,
	pcf8583_stop,
	pcf8583_write,
	pcf8583_read,
	NULL
};

static PCF8583 pcf_s;
static PCF8583 *pcf = &pcf_s; /**< Handle of a PCF8583 state machine */

static I2C_Slave pcf8583_s;
static I2C_Slave *pcf8583 = &pcf8583_s; /**< Handle of the PCF8583 chip I2C slave device */

/****************************************************************************/

#define I2C_SDA	1
#define I2C_SCL	2

#define I2C_STATE_IDLE		0
#define I2C_STATE_ADDR		1
#define I2C_STATE_ACK_READ	2
#define I2C_STATE_ACK_READ_ADDR	3
#define I2C_STATE_ACK_WRITE	4
#define I2C_STATE_NACK_WRITE	5
#define I2C_STATE_READ		6
#define I2C_STATE_WRITE		7
#define I2C_STATE_WAIT		8

/** The I2C state machine */
typedef struct {
	I2C_Slave *active_slave; /* Todo: multiple simultaneously active devices */
	int slave_was_accessed;
	int address;
	uint8_t inbuf;
	uint8_t outbuf;
	int bitcount;
	int state;
	int oldpinstate;
} I2C_SerDes;

static I2C_SerDes serdes_s;
static I2C_SerDes *serdes = &serdes_s; /**< Handle of the I2C state machine */

/**
 * Reset the I2C state machine
 *
 * @param serdes State machine to reset
 */
static void
reset_serdes(I2C_SerDes *serdes)
{
	serdes->state = I2C_STATE_IDLE;
	serdes->bitcount = 0;
	serdes->address = 0;
	serdes->active_slave = NULL;
	serdes->inbuf = 0;
	serdes->outbuf = 0;
	serdes->slave_was_accessed = 0;
	i2cclock = 1;
	i2cdata = 1;
}

/**
 * Handle a 'write' to the I2C bus.
 * Called from IOMD.
 *
 * @param scl Bool of the state of the I2C clock pin
 * @param sda Bool of the state of the I2C data pin
 */
void
cmosi2cchange(int scl, int sda)
{
	int oldscl = serdes->oldpinstate & I2C_SCL;
	int oldsda = serdes->oldpinstate & I2C_SDA;

	dbgprintf("scl %x, sda %x, prev %02x, state %d\n",
	          scl, sda, serdes->oldpinstate, serdes->state);

	/* Detect Start/Repeated condition */
	if (scl && oldsda  && !sda) {
		reset_serdes(serdes);
		dbgprintf("Start Condition\n");
		serdes->state = I2C_STATE_ADDR;

		/* Leaving Function, store current state of i2c pins */
		serdes->oldpinstate = (scl << 1) | sda;
		return;
	}
	/* Stop condition */
	if (scl && !oldsda  && sda) {
		I2C_Slave *slave;
		slave = serdes->active_slave;
		if (slave) {
			slave->devops->stop(slave->dev);
		}
		reset_serdes(serdes);
		dbgprintf("Stop Condition\n");

		/* Leaving Function, store current state of i2c pins */
		serdes->oldpinstate = (scl << 1) | sda;
		return;
	}

	switch (serdes->state) {
	case I2C_STATE_IDLE:
		i2cclock = 1;
		i2cdata  = 1;
		break;

	case I2C_STATE_ADDR:
		if (oldscl && !scl) {
			i2cclock = 1;
			i2cdata  = 1;
		} else if (!oldscl && scl) {
			serdes->bitcount++;
			serdes->inbuf <<= 1;
			if (sda) {
				serdes->inbuf |= 1;
			}
			dbgprintf("inbuf 0x%02x\n", serdes->inbuf);
			if (serdes->bitcount == 8) {
				I2C_Slave *slave = NULL;

				serdes->address = serdes->inbuf >> 1;

				/* Detect which device is being talked to */
				if (serdes->address == pcf8583->address) {
					slave = pcf8583;
				} else {
					fprintf(stderr, "Request for unhandled I2C device %02X\n",
					        serdes->address);
					rpclog("Request for unhandled I2C device %02X\n", serdes->address);
				}

				dbgprintf("I2C-Address %02x slave %p\n",
				          serdes->inbuf >> 1, slave);
				if (slave) {
					int result;

					if (serdes->inbuf & 1) {
						serdes->state = I2C_STATE_ACK_READ_ADDR;
						result = slave->devops->start(slave->dev, serdes->address, I2C_READ);
					} else {
						serdes->state = I2C_STATE_ACK_WRITE;
						result = slave->devops->start(slave->dev, serdes->address, I2C_WRITE);
					}
					if (result == I2C_ACK) {
						serdes->active_slave = slave;
						i2cclock = 1;
						i2cdata  = 1;
					} else if (result == I2C_NACK) {
						i2cclock = 1;
						i2cdata  = 1;
						serdes->state = I2C_STATE_WAIT;
					}
				} else {
					reset_serdes(serdes);
				}
			}
		}
		break;

	case I2C_STATE_ACK_READ_ADDR:
		if (oldscl && !scl) {
			i2cclock = 1;
			i2cdata  = 0;
		} else if (!oldscl && scl) {
			serdes->state = I2C_STATE_READ;
			serdes->bitcount = 8;
			serdes->slave_was_accessed = 1;
		}
		break;

	case I2C_STATE_ACK_READ:
		if (oldscl && !scl) {
			/* release the lines */
			i2cclock = 1;
			i2cdata  = 1;
		} else if (!oldscl && scl) {
			I2C_Slave *slave = serdes->active_slave;

			if (slave == NULL) {
				fprintf(stderr, "I2C-SerDes Bug: no slave in I2C-read\n");
				rpclog("I2C-SerDes Bug: no slave in I2C-read\n");
				exit(5324);
			}
			if (sda) {
				/* Last was not acknowledged, so read nothing */
				dbgprintf("Not acked\n");
				serdes->state = I2C_STATE_WAIT;
				/*
				 * -------------------------------------------
				 * forward nack to device for example
				 * to trigger a NACK interrupt
				 * -------------------------------------------
				 */
				if (slave->devops->read_ack) {
					slave->devops->read_ack(slave->dev, I2C_NACK);
				}
				if (!i2cdata) {
					fprintf(stderr, "Emulator Bug in %s line %d\n", __FILE__, __LINE__);
					rpclog("Emulator Bug in %s line %d\n", __FILE__, __LINE__);
				}
			} else {
				/*
				 * -------------------------------------------
				 * forward ack to device because it
				 * can start preparing next data byte for read
				 * -------------------------------------------
				 */
				if (slave->devops->read_ack) {
					slave->devops->read_ack(slave->dev, I2C_ACK);
				}
				serdes->state = I2C_STATE_READ;
				serdes->bitcount = 8;
				serdes->slave_was_accessed = 1;
			}
		}
		break;

	case I2C_STATE_ACK_WRITE:
	case I2C_STATE_NACK_WRITE:
		if (oldscl && !scl) {
			i2cclock = 1;
			i2cdata  = 0;
			if (serdes->state == I2C_STATE_NACK_WRITE) {
				i2cdata = 1;
			}
			break;
		} else if (!oldscl && scl) {
			dbgprintf("goto write state addr %02x\n", serdes->address);
			serdes->state = I2C_STATE_WRITE;
			serdes->bitcount = 0;
			serdes->inbuf = 0;
			break;
		}
		break;

	case I2C_STATE_READ:
		/* We change output after falling edge of scl */
		if (oldscl && !scl) {
			I2C_Slave *slave = serdes->active_slave;
			if (serdes->bitcount == 8) {
				(void) slave->devops->read(slave->dev, &serdes->outbuf);
			}
			if (serdes->bitcount > 0) {
				serdes->bitcount--;
				i2cclock = 1; // should be delayed
				i2cdata  = 0;
				if ((serdes->outbuf & (1 << serdes->bitcount)) != 0) {
					i2cdata = 1;
				}
			} else {
				fprintf(stderr, "I2C_SerDes Bug: bitcount<=0 should not happen\n");
				rpclog("I2C_SerDes Bug: bitcount<=0 should not happen\n");
			}
		} else if (!oldscl && scl) {
			if (serdes->bitcount == 0) {
				serdes->state = I2C_STATE_ACK_READ;
			}
		}
		break;

	case I2C_STATE_WRITE:
		if (oldscl && !scl) {
			i2cclock = 1;
			i2cdata  = 1;
		} else if (!oldscl && scl) {
			serdes->bitcount++;
			serdes->inbuf <<= 1;
			if (sda) {
				serdes->inbuf |= 1;
			}
			if (serdes->bitcount == 8) {
				I2C_Slave *slave;
				int result;

				slave = serdes->active_slave;
				if (slave == NULL) {
					fprintf(stderr, "I2C-SerDes Emulator Bug: no slave in write\n");
					rpclog("I2C-SerDes Emulator Bug: no slave in write\n");
					exit(3245);
				}
				result = slave->devops->write(slave->dev, serdes->inbuf);
				serdes->slave_was_accessed = 1;
				if (result == I2C_ACK) {
					serdes->state = I2C_STATE_ACK_WRITE;
					i2cclock = 1;
				} else if (result == I2C_NACK) {
					serdes->state = I2C_STATE_NACK_WRITE;
					i2cclock = 1;
				} else {
					fprintf(stderr, "Bug: Unknown I2C-Result %d\n", result);
					rpclog("Bug: Unknown I2C-Result %d\n", result);
				}
			}
		}
		break;

	case I2C_STATE_WAIT:
		break;

	default:
		fprintf(stderr, "I2C_SerDes: Bug, no matching handler for state %d\n", serdes->state);
		rpclog("I2C_SerDes: Bug, no matching handler for state %d\n", serdes->state);
		i2cclock = 1;
		i2cdata  = 1;
	}

	/* Leaving Function, store current state of I2C pins */
	serdes->oldpinstate = (scl << 1) | sda;
}

void reseti2c(void)
{
	i2cclock = 1;
	i2cdata = 1;

	/* Prepare the Philips Real Time Clock chip slave device */
	pcf8583->devops = &pcf8583_ops;
	pcf8583->address = 0x50;
	pcf8583->dev = pcf;

	/* Initialise the I2C state machine */
	reset_serdes(serdes);
}
