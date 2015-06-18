# Overview
This PCB is a breakout board to be used for prototyping a thermal printer with the BeagleBone Black. It utilizes 30-pin and 8-pin FPC connectors to interface with a Fujitsu FTP-628MCL401 print head. The breakout board breaks these print head signals out to standard 0.1" header pins, designated by "HEAD_OUT" and "CUT_OUT".

The board also contains a standard BeagleBone EEPROM for storing cape information and headers for accessing the FTDI serial RX/TX when the cape is mounted on a BBB. 


# Components
* 30-pin FPC connector; Molex 52610-3072
* 8-pin FPC connector; Molex 52610-0872
* 4-pin 2x2 surface mount female 0.1" header; Samtec SSM-102-S-DV
* 30-pin male 0.1" header for print head connector
* 8-pin male 0.1" header for cutter connector
* 5-pin male 0.1" header for USB-FTDI cable
* 2x26 0.1" header for BBB expansion (2)

* BBB cape EEPROM; CAT24C256WI-GT3
* 5.6k-ohm 0402 pull-up resistor (2)
* 4.75k-ohm 0402 pull-up resistor
* 0.1uF 0402 decoupling capacitor


# Gerbers
Gerbers and drill hole files are available in the "/Project Outputs..." directory. These design files have been uploaded to OSH Park and can be directly [purchased](https://oshpark.com/shared_projects/kP1HCuRb) for $31.15 (for 3 boards).
