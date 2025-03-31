# z80pack on ESP32-2432S028R a.k.a. Cheap Yellow Display (CYD)

**This is currently a basic port to the CYD without any display output or touch input.
Hopefully this will change in the future...**

# Emulation

z80pack on the ESP32-2432S028R device emulates a Intel 8080 / Zilog Z80 system
from ca. 1976 with the following components:

- 8080 and Z80 CPU, switchable
- 112 (352) KB RAM, two banks with 48 KB and a common segment with 16 KB
- 256 bytes boot ROM with power on jump in upper most memory page
- MITS Altair 88SIO Rev. 1 for serial communication with a terminal
- DMA floppy disk controller
- four standard single density 8" IBM compatible floppy disk drives

Disk images, standalone programs and virtual machine  configuration are saved
on a MicroSD card, plugged into the device.

The virtual machine can run any standalone 8080 and Z80 software, like
MITS BASIC for the Altair 8800, examples are available in directory
src-examples. With a bootable disk in drive 0 it can run these
operating systems:

- CP/M 2.2
- CP/M 3 banked, so with all features enabled
- UCSD p-System IV
- FIG Forth 8080 using drive 1 as block device, so true operating system

All implemented operating systems use 8080 instructions only, so it is
possible to switch CPU's anytime, even 'on the fly'.

# Building

To build z80pack for this device you need to have the SDK 5.4.1 for ESP32
based devices installed and configured. The SDK manual has detailed
instructions how to install on all major PC platforms, it is available here:
[ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html)

Then clone the GitHub repositories:

1. clone z80pack: git clone https://github.com/udo-munk/z80pack.git
2. checkout dev branch: cd z80pack; git checkout dev; cd ..
3. clone this: git clone https://github.com/sneakywumpus/cyd-80.git

To build the application for the ESP32-2432S028R:
```
cd cyd-80
idf.py build
```

Flash into the device with `idf.py flash`.

# Preparing MicroSD card

In the root directory of the card create these directories:
```
CONF80
CODE80
DISKS80
```

Into the CODE80 directory copy all the .bin files from src-examples.
Into the DISKS80 directory copy the disk images from disks.
CONF80 is used to save the configuration, nothing more to do there,
the directory must exist though.

# Optional features

A feature one might be missing is,
that z80pack also contains a Mostek In Circuit Emulator (ICE). Per default
it is disabled, because we assume that those just using it don't
want to mess with the sources, and just use the little machines to run
some software on. For the software engineer trying to find some nasty problem
on a true Von Neumann can be a challenge, and we have tools like debuggers
for this. The ICE can be enabled with a commented define in srcsim/sim.h,
and gives one everything needed, like software breakpoints, hardware
breakpoint, single step, dump of memory and CPU registers and so on.
