//Repository lives at: https://gitorious.design.ti.com/pruprinter
//Video demonstration: https://youtu.be/tVr5VrtNZiQ

AM335x PRU-ICSS-based Thermal Printer Demo and Low-Level Driver
===============================================================
Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
ALL RIGHTS RESERVED

Written by Andreas Dannenberg (dannenberg@ti.com)

Overview
--------
This project demonstrates how the AM335'x PRU-ICSS can be used to directly
drive and interface with a thermal printer assembly printing out bitmap
images. It comprises a low-level firmware that runs independently from the
main Linux CPU inside the PRU-ICSS and generates the neccessary signal waveforms
to drive the printer and a Linux command line application that reads PNG-
formatted monochrome bitmap images, processes them into a "print job", and
passes them to the low-level PRU-ICSS firmware for printing.

The software is designed such that it runs on a BeagleBone (white) EVM, with
all signals needed to interface with the printer assembly available on the
BeagleBone's P8 and P9 extension headers.

File System Organization
------------------------
demo_images/            Some PNG images that can be printed for demo purposes
doc/                    Various associated documentation/datasheets
kernel_support/         Kernel patch to configure the pinmux needed for PRU-ICSS
pcb/                    Hardware design information
src/                    Project source code folder
src/pruprint/           Linux host command-line application CCSv6.0 project
src/pruprinter_fw/      PRU-ICSS unit 1 low-level thermal printer driver
                        CCSv6.0 project.

Prerequisites
-------------
The project is based on TI's AM335x SDK 06_00_00_00 developer package and uses
the "Texas Instruments PRUSS driver" userspace I/O driver for communication with
the PRU-ICSS. Also, a small modification to the BSP source is needed to enable
the needed pinmix PRU-ICSS configuration during boot. In order to enable support
for this project the Kernel has to be build using the following steps:

(1) Enter the TI AM335x SDK root folder
$ cd ti-sdk-am335x-evm-06.00.00.00

(2) Apply the provided patch in the "kernel_support" folder to the Linux and BSP
tree
$ patch -p1 < <pruprinter-install-dir>/kernel_support/thermal-printer-bsp.patch

(3) Start the Kernel configuration and build process
$ make

Navigate to Device Drivers --> Userspace I/O drivers
Select the "Texas Instruments PRUSS  driver" to be build into the Kernel.
Select "Exit" to save the configuration and start building the new Kernel image.

(4) Copy/install the Kernel image into the appropriate location (SD-Card or NFS
folder)

Building the PRU Printer Project
--------------------------------
The project was developed and tested using the GA version of TI's Code
Composer Studio v6.0.0.00190 as well as version v2.0.0.B2 of TI's PRU-ICSS C
compiler obtained via the CCS app store. To build the project import and build
both 'pruprinter_fw' and 'pruprint' sub-projects into Code Composer. Note that
building the low-level firmware using 'pruprinter_fw' involves a custom build
step that requires the srec_cat tool to be installed in the system which is
part of the SRecord converter tool which can be installed by executing
'sudo apt-get install srecord' in the shell. Also note that the 'pruprinter_fw'
should be build as its output is consumed by 'pruprint'.

TODO
----
* Add more detailed documentation and code flow description
* Provide ready-to-use CCS debug configurations for easier debug session setup
* Create a Kernel patch that configures the PRU-ICSS signals used for the
  printer to safe defaults _before_ the pin mux configuration is changed to
  PRU-ICSS (to prevent potential erroneous and/or dangerous driving of the
  printer head/circuit)
* Optimize/verify signal timing based on the specific thermal printer assembly
  to be used

