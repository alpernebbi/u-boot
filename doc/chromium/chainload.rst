.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2020 Google LLC

Running U-Boot from coreboot on Chromebooks
===========================================

U-Boot can be used as a secondary boot loader in a few situations such as from
UEFI and coreboot (see README.x86). Recent Chromebooks use coreboot even on
ARM platforms to start up the machine.

This document aims to provide a guide to booting U-Boot on a Chromebook. It
is only a starting point, and there are many guides on the interwebs. But
placing this information in the U-Boot tree should make it easier to find for
those who use U-Boot habitually.

Most of these platforms are supported by U-Boot natively, but it is risky to
replace the ROM unless you have a servo board and cable to restore it with.


For all of these the standard U-Boot build instructions apply. For example on
ARM::

   sudo apt install gcc-arm-linux-gnueabi
   mkdir b
   make O=b/nyan_big CROSS_COMPILE=arm-linux-gnueabi- nyan-big_defconfig all

You can obtain the vbutil_kernel utility here:

   https://drive.google.com/open?id=0B7WYZbZ9zd-3dHlVVXo4VXE2T0U


Binman-generated partition images
---------------------------------

Binman generates partition images ("u-boot-depthcharge.kpart") that should
make the Chrome OS bootloader chainload into U-Boot for the following
boards:

- chromebook_bob
- chromebook_kevin
- chromebook_speedy
- chromebook_minnie
- chromebook_jerry
- chromebit_mickey
- nyan-big
- snow
- spring
- peach-pi
- peach-pit

The firmware on these boards chooses its payload based on the "Chrome OS
Kernel" type GPT partitions. You can prepare an SD card or a USB drive
on another machine::

   DISK=/dev/sdc   # Replace with your actual SD card device
   sudo cgpt create $DISK
   sudo cgpt add -b 34 -s 32768 -P 1 -S 1 -t kernel $DISK
   sudo cgpt add -b 32802 -s 2000000 -t rootfs $DISK
   sudo gdisk $DISK   # Enter command 'w' to write a protective MBR to the disk

And write this partition image to the partition::

   sudo dd if=u-boot-depthcharge.kpart of=${DISK}1; sync

Reboot the target board in dev mode, into ChromeOS. Make sure that you have USB
booting enabled by logging in as root (via Ctrl-Alt-forward_arrow) and typing
'enable_dev_usb_boot'. You only need to do this once.

Reboot the board again with the SD card inserted. Press Ctrl-U at the developer
mode screen. It should show U-Boot output on the display.


Enabling more boards
--------------------

If you want to add a new chromebook board to U-Boot, you can enable building
these images for it with:

- Select HAS_DEPTHCHARGE from its TARGET_<board> config option
- Set CONFIG_DEPTHCHARGE_PARTITION_IMAGE=Y in its defconfig
- Include "depthcharge-u-boot.dtsi" in its "<board>-u-boot.dtsi" file


How chromebooks use coreboot
----------------------------

Coreboot itself is not designed to actually boot an OS. Instead, a program
called Depthcharge is used. This originally came out of U-Boot and was then
heavily hacked and modified such that is is almost unrecognisable. It does
include a very small part of the U-Boot command-line interface but is not
usable as a general-purpose boot loader.

In addition, it has a very unusual design in that it does not do device init
itself, but instead relies on coreboot. This is similar to (in U-Boot) having
a SPI driver with an empty probe() method, relying on whatever was set up
beforehand. It can be quite hard to figure out between these two code bases
what settings are actually used. When chain-loading into U-Boot we must be
careful to reinit anything that U-Boot expects. If not, some peripherals (or
the whole machine) may not work. This makes the process of chainloading more
complicated than it could be on some platforms.

Finally, it supports only a subset of the U-Boot's FIT format. In particular
it uses a fixed address to load the FIT and does not support load/exec
addresses. This means that U-Boot must be able to boot from whatever
address Depthcharge happens to use (it is the CONFIG_KERNEL_START setting
in Depthcharge). In practice this means that the data in the kernel@1 FIT node
(see above) must start at the same address as U-Boot's CONFIG_TEXT_BASE.
But this can be worked around by building U-Boot as position-independent.

On ARM64 boards, Depthcharge loads the kernel image within the FIT to a
dynamically chosen location, so it must be position-independent. It also
checks for the Linux kernel image header which must also be enabled.


Manually building images for ARM boards
---------------------------------------

The nyan-big board is used as an example here, but the general process is
the same on ARM boards.

Compiled based on information here::

   https://lists.denx.de/pipermail/u-boot/2015-March/209530.html
   https://git.collabora.com/cgit/user/tomeu/u-boot.git/commit/?h=nyan-big
   https://lists.denx.de/pipermail/u-boot/2017-May/289491.html
   https://github.com/chromeos-nvidia-androidtv/gnu-linux-on-acer-chromebook-13#copy-data-to-the-sd-card

1. Build U-Boot

Steps::

   mkdir b
   make -j8 O=b/nyan-big CROSS_COMPILE=arm-linux-gnueabi- nyan-big_defconfig all


2. Select a .its file

Select something from doc/chromium which matches your board, or create your
own.

Note that the device tree node is required, even though it is not actually
used by U-Boot. This is because the Chromebook expects to pass it to the
kernel, and crashes if it is not present.


3. Build and sign an image

Steps::

   ./b/nyan-big/tools/mkimage -f doc/chromium/files/nyan-big.its u-boot-chromium.fit
   echo test >dummy.txt
   vbutil_kernel --arch arm \
     --keyblock doc/chromium/files/devkeys/kernel.keyblock \
     --signprivate doc/chromium/files/devkeys/kernel_data_key.vbprivk \
     --version 1 --config dummy.txt --vmlinuz u-boot-chromium.fit \
     --bootloader dummy.txt --pack u-boot.kpart

To check that you copied the u-boot.its file correctly, use these commands.
You should see that the data at 0x100 in u-boot-chromium.fit is the first few
bytes of U-Boot::

   hd u-boot-chromium.fit |head -20
   ...
   00000100  b8 00 00 ea 14 f0 9f e5  14 f0 9f e5 14 f0 9f e5  |................|

   hd b/nyan-big/u-boot.bin |head
   00000000  b8 00 00 ea 14 f0 9f e5  14 f0 9f e5 14 f0 9f e5  |................|


The 'data' property of the FIT is set up to start at offset 0x100 bytes into
the file. The change to CONFIG_TEXT_BASE is also an offset of 0x100 bytes
from the load address. If this changes, you either need to modify U-Boot to be
fully relocatable, or expect it to hang.


Positioning in Depthcharge's kernel region
------------------------------------------

It's possible to precisely position U-Boot in the FIT image to ensure
Depthcharge places it exactly at CONFIG_TEXT_BASE, but it's necessary to
figure out the proper KERNEL_START and KERNEL_SIZE values for your board.

First, find out the code name, board name and the base board. There are
several ways to do this, but most comprehensive resource here (besides going
through the sources) is the `Chrome OS docs`_:

.. _Chrome OS docs: https://www.chromium.org/chromium-os/developer-information-for-chrome-os-devices

Also find your firmware version by either pressing TAB in the developer mode
warning screen, or running "sudo crosssystem fwid".

For example, for a Samsung Chromebook Plus, we have::

    Code name: Kevin
    Board name: Kevin
    Base board: Gru
    fwid: Google_Kevin.8785.B

Search the `Depthcharge source code`_ for a firmware-\* branch that matches
these names and firmware version. In this branch, find the defconfig file
for your board in the board directory (e.g "board/kevin/defconfig" in
"firmware-gru.8785.B") which should have KERNEL_START and maybe KERNEL_SIZE.
If not, check other Kconfig files (currently src/image/Kconfig) for the
defaults.

.. _Depthcharge source code: https://chromium.googlesource.com/chromiumos/platform/depthcharge/+refs

Here are some values for boards already in U-Boot::

    Board             | Branch (firmware-*)     | KERNEL_START | KERNEL_SIZE
    ------------------+-------------------------+--------------+-------------
    chromebook_bob    | gru-8785.B              | 0x15000000   | 0x02000000
    chromebook_kevin  |  "                      |  "           |  "
    chromebook_speedy | veyron-6588.B           | 0x02000000   | 0x01000000
    chromebook_minnie |  "                      |  "           |  "
    chromebook_jerry  |  "                      |  "           |  "
    chromebit_mickey  |  "                      |  "           |  "
    nyan-big          | nyan-5771.B             | 0x81000000   | 0x01000000


The very first ARM chromebooks use a `modified U-Boot build`_, with values
that might not be the same in Depthcharge sources::

    Board             | Branch (firmware-*)     | KERNEL_START | KERNEL_SIZE
    ------------------+-------------------------+--------------+-------------
    snow              | snow-2695.B             | 0x42000000   | 0x00800000
    spring            | spring-3824.B           | 0x42000000   | 0x00800000
    peach-pit         | pit-4482.B              | 0x20008000   | 0x00800000
    peach-pi          |  "                      |  "           |  "

.. _modified U-Boot build: https://chromium.googlesource.com/chromiumos/third_party/u-boot/+refs

Set CONFIG_TEXT_BASE to somewhere after KERNEL_START, but within KERNEL_SIZE
bytes of it. Usually KERNEL_START + 0x100 is fine. Then, make sure U-Boot
is at this offset in the 'vmlinuz' file you pass to vbutil_kernel::

   hd u-boot-chromium.fit |head -20
   ...
   00000100  b8 00 00 ea 14 f0 9f e5  14 f0 9f e5 14 f0 9f e5  |................|

   hd b/nyan-big/u-boot.bin |head
   00000000  b8 00 00 ea 14 f0 9f e5  14 f0 9f e5 14 f0 9f e5  |................|

For 32-bit ARM boards, this used to be done by padding the FIT description
with whitespaces in doc/chromium/files/\*.its files.


Empirically deducing/verifying KERNEL_START
-------------------------------------------

If you can get Depthcharge to boot Linux on your ARM board, put an initrd in
the FIT image you build, check the /sys/firmware/chosen/linux,initrd-start
file for the offset of the initrd in the FIT image, then subtract it from
that value, and you should get KERNEL_START.

::

    $ hd /sys/firmware/devicetree/base/chosen/linux,initrd-start
    00000000  15 8f 31 14                                       |..1.|
    00000004
    # gives you 0x158f3114

    $ hd initrd.img | head -1
    00000000  1f 8b 08 00 00 00 00 00  00 03 cc 5c 5b 6c 2c c9  |...........\[l,.|

    $ hd depthcharge.fit | grep -A1 "1f 8b 08 00"         # or something like this
    008f3110  00 00 00 2a 1f 8b 08 00  00 00 00 00 00 03 cc 5c  |...*...........\|
    008f3120  5b 6c 2c c9 59 1e 29 4a  76 67 36 bb 49 f6 02 42  |[l,.Y.)Jvg6.I..B
    # gives you 0x008f3114

    # KERNEL_START = 0x158f3114 - 0x008f3114 = 0x15000000


Other Notes
===========

Nyan-big
--------

On the serial console the word MMC is chopped at the start of the line::

   C:   sdhci@700b0000: 2, sdhci@700b0400: 1, sdhci@700b0600: 0

This is likely due to some problem with change-over of the serial driver
during relocation (or perhaps updating the clock setup in board_init()).

flashrom
~~~~~~~~

Used to make a backup of your firmware, or to replace it.

See: https://www.chromium.org/chromium-os/packages/cros-flashrom


