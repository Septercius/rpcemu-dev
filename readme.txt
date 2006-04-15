RPCemu v0.3
~~~~~~~~~~~

RPCemu is a RiscPC emulator for Win32. At the moment it emulates an ARM7500
class machine - basically an A7000 - or a RiscPC 600/700 without VRAM. It 
also supports two disc drives and an IDE hard drive.


Changes from v0.2 :
~~~~~~~~~~~~~~~~~~~

- Fixed up various 32-bit problems in the ARM core, XXXX XX X now works
- IDE hard disc emulation
- Fixed bugs in floppy emulation
- Added emulation of ARM610 and ARM710
- Low video resolutions (eg 320x480) now supported better
- Faked sound interrupts (not enough for !Replay, but enough to fool Dune II)

At the moment RPCemu is unoptimised and has quite a few bugs, but is useable
as long as the program you are tring to run fits on a floppy and runs in a
supported video mode.


ROMs :
~~~~~~

RISC OS is a commercial product, please do not ask me to provide ROM images, as I
won't. However, they can be obtained from a real RiscPC/A7000 with the following 
commands :

*save ic24 3800000+100000
*save ic25 3800000+100000
*save ic26 3800000+100000
*save ic27 3800000+100000

or alternatively

*save rom  3800000+400000

On later RISC OS versions you may need to type

*save rom  3800000+600000

or possibly even

*save rom  3800000+800000

The ROM files need to be placed in the roms directory.
RISC OS 3.6, 3.7 and X.XX have been tested and work, not sure about any other 
versions.
RPCemu will load in the disc images 'boot.adf' and 'notboot.adf' into :0 and :1
on startup if they exist, so that you can boot straight away. (the CMOS is
configured for this). It will also load in hd4.hdf straight on startup.

Floppy discs can sometimes be corrupted when they've been written to - so keep backups!
IDE emulation appears to be fine though.


Emulates :
~~~~~~~~~~

ARM610/ARM710/ARM7500 (selectable)
4-16mb RAM
Two high density 3.5" disc drives
One IDE hard drive
VIDC20
IOMD
PS/2 keyboard and mouse


Doesn't emulate :
~~~~~~~~~~~~~~~~~

Sound
VRAM
Anything else

Performance on my system (AXP/2400) is useable, about the same speed as an
A7000 (around 16/17 MIPS on Windows 98, 18/19 MIPS on Windows XP). 
The performance partly depends on your graphics card - I noticed a doubling in 
speed after installing decent AGP drivers. It doesn't suffer from the usual 
drawbacks of an ARM7500 system as RPCemu doesn't emulate the slowdown associated 
with using DRAM for video.
The only video modes supported at the moment are 2, 4, 16 and 256 colours, up to
1024x768. The cursor sometimes has the wrong colours at the moment.


Hard disc emulation :
~~~~~~~~~~~~~~~~~~~~~

You will need to format the hardfile to make use of it. !HDFormat will do the
job. There are a couple of things to note when formatting :

- The hard disc must always have 63 sectors and 16 heads
- The number of cylinders can vary, the default 101 gives about 50 megs
- Never run a long soak test on the hard disc - this does not work!

Once formatted you can copy the boot sequence from floppy (or just download
the preprepared hardfile). To get RISC OS to boot off hard disc, type the 
following commands at the command prompt :

*dir adfs::4.$
*opt 4,2
*configure drive 4


Other things :
~~~~~~~~~~~~~~

When switching between ARM610/ARM710 and ARM7500, you may need to reconfigure
the mouse type (depending on what OS you are running). Type

*configure mousetype 0

for ARM610/ARM710 and

*configure mousetype 3

for ARM7500.

RISC OS 3.6 and 3.7 sometimes have trouble reading the CMOS data. XXXX XX X seems 
to not suffer from this bug.

It's entirely possible that some OSes won't like ARM610/ARM710 mode - if it
hangs during startup try switching to ARM7500 mode.


Todo list :
~~~~~~~~~~~

Implement VRAM - it can be enabled in the source, but RISC OS then crashes on
startup
Fix bug preventing more than 16MB of RAM - RISC OS will hang during startup
if this is the case
Optimise memory accesses further
Finish VIDC20 support - implement other resolutions/colour depths and sound


Programs tested :
~~~~~~~~~~~~~~~~~

Blocks
Meteors
Minehunt
Patience
Syndicate (demo)
Wizard Apprentice (demo) (with a couple of graphics issues)
Sidewinder
Artworks Viewer
VKiller
Extend virus
FreeDoom (keyboard problems)
Dune II
Bubble Impact (slow + flickery)


Tom Walker
tommowalker@yahoo.co.uk
b-em@bbcmicro.com