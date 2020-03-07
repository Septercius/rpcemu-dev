This is a development repository for **RPCEmu**, an Acorn RISC PC emulator for Windows, Mac and Linux.  It is intended for use in developing and testing patches before they are submitted to the mailing list for inclusion in the original source tree.

The current version of RPCEmu, and the version upon which these patches are built, is 0.9.2.

The home page for RPCEmu can be found here: http://www.marutan.net/rpcemu/.

This repository contains the following patches:

* Version 4 of the main OS X patch.

The sections below outline each patch in more detail.

## OS X patch - version 4 

This patch provides the following:

* Keyboard support (required due to the way that QT exposes keyboard information).
* Network support, using the new SLIRP functionality added in 0.9.2.  This enables use of email, FTP, the web and so on.
* Dynamic compilation support for later versions of OS X (including High Sierra, Mojave and Catalina).
* Configurable data folder setting, allowing the application to reside in a different folder to its settings.
* A non-Mac specific fix for an issue with locating the Ethernet driver (kindly provided by David Pitt).
* A non-Mac specific fix for processing of mouse events when the application is terminating.

