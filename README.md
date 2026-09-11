This is a repository for **RPCEmu**, an Acorn RISC PC emulator for Windows, Mac and Linux.  Its main purpose is to provide a build for users of macOS, as these are not available directly from the RPCEmu home page (http://www.marutan.net/rpcemu/).

The basis for this repository is version 0.9.5 of RPCEmu.  It has been modified to support Macs and (as of version 0.9.5b), now includes support for multiple drives using HostFS.

On starting the emulator for the first time, you will be prompted to select the location you wish to use to store its data.  A standard folder can be found in the DMGs and ZIPs for each release, named "Data".  This is where ROM files must be placed, and is also where the emulator stores its configuration file.  You should copy the "Data" folder to a suitable location if you don't have one from a previous installation, and then use the "Select" button to browse to it.  The emulator should then start as normal.

## Multiple drive support for HostFS

The standard build of RPCEmu allows a single drive for HostFS, and its location is hard-coded to be the "hostfs" folder within the RPCEmu data directory.  Version 0.9.5b in this repository allows up to four drives to be selected, and the location of each to be specified individually by you.

The HostFS configuration can be accessed using the "Settings > HostFS" menu.  Each drive can be given its own name, and the relevant folder can be specified.  There are check boxes at the bottom of the window that control whether system files (e.g. "/System") and dot files (e.g. ".zshrc") are shown within the HostFS filer windows.

:warning: **IMPORTANT** :warning: **If you are updating from a previous version of RPCEmu from this repository, you will need to delete the existing `hostfs,ffa` and `hostfsfiler.ffa` files from the `poduleroms` sub-folder and replace them with the `multihostfs,ffa` and `multihostfsfiler,ffa` files from the `Data` folder of a release that supports multiple drives (0.9.5b and later).  Multiple drive HostFS will not work without these files.**

## QT6 user interface

The user interface components have been converted to QT6.  Binary releases are built against version 6.11.1 of QT.

## Additional changes to support macOS

This patch provides the following:

* Keyboard support (required due to the way that QT exposes keyboard information).
* Network support, using the new SLIRP functionality added in 0.9.2.  This enables use of email, FTP, the web and so on.
* Dynamic compilation support for later versions of OS X (High Sierra and newer).
* Configurable data folder setting, allowing the application to reside in a different folder to its settings.
* A non-Mac specific fix for an issue with locating the Ethernet driver (kindly provided by David Pitt).
* A change to the key combination for exiting mouse capture mode from CTRL+END to CTRL+COMMAND.  This is aimed at laptop users, who do not have a dedicated END key.
