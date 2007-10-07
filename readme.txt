RPCemu v0.5
~~~~~~~~~~~

RPCemu is a RiscPC emulator for Win32. At the moment it emulates an ARM7500
class machine - basically an A7000 - or a RiscPC 600/700 with or without VRAM. 
It also supports two disc drives and two IDE hard drives.

RPCemu is licensed under the GPL, see COPYING for more details.


Changes from v0.4 :
~~~~~~~~~~~~~~~~~~~

- Optimised flag emulation and optimised outer loop - 30% speed increase
- Couple of new instructions - latest Artworks Viewer now runs
- Bugfix in IOMD timers - !ArcQuake and !SICK now work
- Bugfix in MMU - large memory configurations work with all OSes now
- Bugfix in dirty buffering - no more display artifacts
- Windows interface slightly better
- Added workaround for blitting issues on some graphics cards
- Preliminary sound emulation (Windows 9x only)


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

The ROM files need to be placed in the roms directory.
RISC OS 3.6, 3.7 and X.XX have been tested and work, not sure about any other 
versions.
RPCemu will load in the disc images 'boot.adf' and 'notboot.adf' into :0 and :1
on startup if they exist, so that you can boot straight away. (the CMOS is
configured for this). It will also load in hd4.hdf and hd5.hdf straight on startup.

RPCEmu emulates a podule containing up to 4MB of ROM which can be used to hold 
additional RISC OS modules. To use this ROM, create a poduleroms directory, 
and copy any modules into this directory. These modules are then active before
RISC OS boots, and so can be used for things like booting off HostFS.

Emulates :
~~~~~~~~~~

ARM610/ARM710/ARM7500 (selectable)
4-128mb RAM
Up to 2mb VRAM
Two high density 3.5" disc drives
Two IDE hard drive
VIDC20
IOMD
PS/2 keyboard and mouse
16-bit sound (preliminary)


Doesn't emulate :
~~~~~~~~~~~~~~~~~

CD-ROM drives
Networking
Anything else


Speed :
~~~~~~~

An Athlon 1.2ghz system delivers similar performance to an ARM610 RiscPC.
My Athlon XP 2400 performs slightly faster than an ARM710 (around 23/24 MIPS). !SICK reports 
around 60000 Dhrystones/sec, which is a bit faster than an ARM710. It also reports a memory bus 
around three times faster than a RiscPC.

The emulator runs faster on an Athlon than a P4, A P4 2.6ghz with otherwise superior 
components gets about 14/15 MIPS.

The performance partly depends on your graphics card - I noticed a doubling in 
speed after installing decent AGP drivers. It doesn't suffer from the usual 
drawbacks of an ARM7500 system as RPCemu doesn't emulate the slowdown associated 
with using DRAM for video.


Hard disc emulation :
~~~~~~~~~~~~~~~~~~~~~

You will need to format the hardfile to make use of it. !HForm will do the
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


HostFS :
~~~~~~~~

HostFS always reads from the hostfs directory. Filetypes are represented as 3 digit
hex numbers after a comma, or a pair of load/exec addresses.

I am not aware of any bugs with HostFS, however if you come across any let me know.

It is possible to boot from HostFS. To do so, copy the HostFS module into the poduleroms
directory, so that it gets loaded in to ROM, then
*configure filesystem HostFS


Sound :
~~~~~~~

RPCemu attempts to emulate the 16-bit sound system. Unfortunately due to various issues
of timing and bad sound card drivers, performance varies wildly. With my sound card
(Aureal Vortex AU8820) the sound is reasonable on Windows 98 but sounds horrible on XP.
Even if it does work correctly on your system there may still be issues with some 
soundtracker modules due to the audio interrupts not being evenly spaced.

The sound code is multithreaded, hence performance may be different if you have two
processors / a multicore processor / hyperthreading. Since I don't have a system with
any of these I have been unable to test.


Other things :
~~~~~~~~~~~~~~

When switching between ARM610/ARM710 and ARM7500, you may need to reconfigure
the mouse type (depending on what OS you are running). Hit F12 and type

*configure mousetype 0

for ARM610/ARM710 and

*configure mousetype 3

for ARM7500.


RPCemu requires your desktop to be in either 16 or 32 bit colour. I don't think this
is unreasonable - I had to go back to 1999 to find a graphics card that actually 
supported 24-bit colour, and I suspect very few people will run in this depth anyway.

Obviously if your desktop is in 16 bit colour there is very little point in putting
RISC OS into 32 bit colour.

There appears to be a bug relating to vertical scaling and some graphics cards (at least 
my GeForce 256). If you notice any bizarre problems with video in some modes then enable 
'Alternative blitting code' in the Settings menu. This does have a speed hit however. 
This isn't a common problem, no ATI or 3dfx cards I tried suffer from this, nor does a
GeForce 3.


Todo list :
~~~~~~~~~~~

Optimise more. I'm toying with rewriting the ARM core in assembler.

Make sound work better.

Add/finished StrongARM emulation (removed from this release due to being incomplete).
I was toying with putting it back in for this release (it's useful for AMPlayer) but
it slows down the emulation overall and is a little broken (with it enabled ArcQuake
will always run with StrongARM instructions totally broken, even in ARM610/ARM710
mode).


Programs tested :
~~~~~~~~~~~~~~~~~

Programs that worked :

Blocks
Meteors
Minehunt
Patience
Syndicate (demo)
Wizard Apprentice (demo)
Chaos Engine (demo)
Lemmings 2 (lacks palette splitting though - use Arculator instead!)
Sidewinder
Artworks Viewer
VKiller
Extend virus
FreeDoom
Dune II
Bubble Impact
ScummVM
AMPlayer
ArcQuake
OpenTTD
LongFiles
Alone In The Dark (demo)


Tom Walker
tommowalker@yahoo.co.uk
b-em@bbcmicro.com