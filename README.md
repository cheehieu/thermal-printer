<img src="http://niftyhedgehog.com/thermal-printer/images/printer_prototype.jpg">

## Overview
This project is a thermal printer reference design featuring the PRU-ICSS on the AM335x. It was inspired by a similar project by [Andreas Dannenberg](https://gitorious.design.ti.com/pruprinter), who prototyped a functioning thermal printer out of a gutted POS unit and a BeagleBone Black. Here is a video of his prototype: [https://youtu.be/tVr5VrtNZiQ].

The main intent of this project is to showcase the real-time abilities of the PRU-ICSS for time-sensitive motor control applications. It also serves as a platform to demonstrate the new Linux-PRU software communication interface, . Much of this project's design follows Andreas' design choices (TI components, area- and cost-sensitive), but also incorporates some ideas from the FTP-628DSL491R interface board, which is an EVM for the Fujitsu FTP-628MCL401 print head.

## Hardware
<img src="http://niftyhedgehog.com/thermal-printer/images/TPcape_mockup.jpg">

### BeagleBone Black
The AM335x processor on the BeagleBone Black contains the Programmable Real-Time Unit and Industrial Communication Sub System (PRU-ICSS). The PRU consists of dual 32-bit RISC cores, which operate at 200MHz single cycle and have direct access to general IO. This allows for 100% predictable timing, making the PRU an excellent solution for thermal strobes and motor control. 

In this project, the PRU is designed to handle the print head logic and motor control for both head and cutter stepper motors. I tried to choose logical PRU signal groupings to simplify board layout and software design. Make sure to execute the proper pin-mux changes in the device tree. The PRU signals are connected on the cape as follows:

| BBB Pin |  PRU Signal |      Cape Function      |
|:-------:|:-----------:|:-----------------------:|
| P8.30   | pru1_r30_11 | Vdd supply enable       |
| P8.21   | pru1_r30_12 | VH supply enable        |
| P8.20   | pru1_r30_13 | Buffer output enable    |
| P8.15   | pru0_r31_15 | Motor driver fault HEAD |
| P9.26   | pru1_r31_16 | Motor driver fault CUT  |
| P8.46   | pru1_r30_1  | MTAn_head               |
| P8.45   | pru1_r30_0  | MTA_head                |
| P8.43   | pru1_r30_2  | MTBn_head               |
| P8.44   | pru1_r30_3  | MTB_head                |
| P8.41   | pru1_r30_4  | MTA_cut                 |
| P8.42   | pru1_r30_5  | MTAn_cut                |
| P8.39   | pru1_r30_6  | MTB_cut                 |
| P8.40   | pru1_r30_7  | MTBn_cut                |
| P9.31   | pru0_r30_0  | STB1                    |
| P9.29   | pru0_r30_1  | STB2                    |
| P9.30   | pru0_r30_2  | STB3                    |
| P9.28   | pru0_r30_3  | STB4                    |
| P9.42   | pru0_r30_4  | STB5                    |
| P9.27   | pru0_r30_5  | STB6                    |
| P8.12   | pru0_r30_14 | CLK                     |
| P9.41   | pru0_r30_6  | DI                      |
| P9.25   | pru0_r30_7  | LATn                    |
| P9.24   | pru0_r31_16 | DO                      |

Additional PRU information can be [here](http://processors.wiki.ti.com/index.php/PRU-ICSS), [here](http://elinux.org/Ti_AM33XX_PRUSSv2), and [here](http://www.element14.com/community/community/designcenter/single-board-computers/next-gen_beaglebone//blog/2013/05/22/bbb--working-with-the-pru-icssprussv2).

### Print Head
<img src="http://niftyhedgehog.com/thermal-printer/images/print_head.jpg">

The star of this cape is the thermal print head: the [Fujitsu FTP-628MCL401](http://www.fujitsu.com/downloads/MICRO/fcai/thermal-printers/ftp-628mcl401.pdf). This print head is a high speed thermal printer featuring an auto cutter for full and partial paper cuts. It was selected by Paul Eaves after discussion with a Fujitsu FAE. Note that this is a different print head from Andreas' design. 

One important question that must be answered is the print head's pinout. The print head uses two FPC connectors; a 30-pin for the main print head and an 8-pin for the cutter. It is important to note that the pin assignments in the datasheet do not exactly match those connected on the interface board. I designed this cape using the datasheet pin outs, but it may become evident (during board bringup) to use the ones from the interface board. An "best guess" equivalent pin mapping is included in pru-printer/docs/TPCape_RevA_BOM.xlsx under the "interface" tab.

### Power
During operation, the FTP-628MCL401 print head draws a lot of current. The voltage and current specifications are listed below. In testing, it was possible to run the printer and interface board from the VDD_5V DC input on the BeagleBone Black using a 5VDC, 3A rated wall wart. However, it may have been drawing more than 3A to power the BBB and thermal printer EVM. So, a more robust solution was developed using a 12V supply input.

| Component  | Voltage (V) | Current (A) |
|------------|-------------|-------------|
| Print head |  4.2 - 8.5  |  2.4 peak   |
| Motor      |  4.2 - 8.5  |     1.0     |
| Cutter     | 4.75 - 8.5  |     1.0     |
| Logic      |  3.0 - 5.25 |     0.1     |

I used WEBENCH to create a power solution for connecting a 12V, 5A wall wart (used with the X15). The power solution utilizes a TPS54332 for 12V to 7.2V (3A) conversion to supply VH to the print head. A TPS563209 is used for 12V to 5V (3A) conversion to supply the BeagleBone Black and subsequently the Vdd print head logic supply. These components were chosen for there low BOM cost/count, small footprint, and high efficiency. More details can be found at pru-printer/hw/docs/power/webench/

High side switches are used to control the VH and Vdd supplies. According to the datasheet, all lines connected to the head, including the power supply, must be set to "Low" in order to decrease power consumption during standby. An AND gate is used so that the VH supply will shut off in the event of a system reset.

### Motor Driver
Two DRV8833C motor drivers are utilized to drive the head and cutter stepper motors. This motor driver was selected for its protection features, output current capability, wide power supply voltage range, and its low cost. The PRU provides the control signal outputs.

### Print Sensors
The FTP-628MCL401 contains sensors for enabling additional features of the thermal printer. These include sensors for detecting an open platen, the temperature of the print head, the home position of the cutter, and the paper feed. Most of these sensors output an analog voltage to represent state. The printer datasheet recommends the usage of ADCs to monitor these voltages because they can be unstable.

However, this design utilizes LM393 comparators to trigger at specific threshold voltages created with voltage dividers. These comparators have open-collector outputs and can be wire-ANDed together to create a FAULTn error to the PRU.

### Misc.
Buffers are utilized to ensure signals are robust and can be tri-stated during reset or low-power operation. The SN74LVC244A buffers were selected for its low cost.

A hardware watchdog timer or voltage supervisor is used to ensure the STROBE signals are not active for too long, which would permanently damage the print head.

A standard cape EEPROM is included to store cape and pinmux information. This circuit is nearly identical to the one in the BeagleBone Black SRM. The difference being a fixed I2C address at 0x54, removing the need for a DIP switch.

An FTDI mating header is included to reroute the UART0 RX and TX signals to a more easily accessible connector.

Originally slated for the TPcape "Ultra" version, a Newhaven 2.4" touchscreen display is included for potentially more advanced print features.

### Connectors
The print head connectors used are the Molex 0526103072 (30-pin) and the Molex 0526100872 (8-pin).


## Software
No software is available yet. TI has since made considerable advances on the Linux-PRU front, but is may be useful to review the software work completed by Andreas. See pru-printer/sw/andreas_gitorious/ for the sources.

## Next Steps
# Finish PCB layout
# Order and assemble prototype components
# Board bringup
# Software development
# Implement more complex features like [cloud-connectivity](https://www.adafruit.com/products/1289)
