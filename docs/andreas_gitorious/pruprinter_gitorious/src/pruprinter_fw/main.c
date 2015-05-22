/*
 * main.c
 *
 * AM335x PRU-based Thermal Printer Driver Low-Level Firmware
 *
 * Program waits in a processing loop for a host interrupt to arrive. After
 * it received one it starts processing the print job located in the printer
 * queue in the PRU shared memory. After it's done processing the job it then
 * issues an interrupt back to the host. At this time the host can also read
 * out the status bits located in the printer queue status register.
 *
 * Written by Andreas Dannenberg, 01/01/2014
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 * ALL RIGHTS RESERVED
 */

// TODO: Add functionality to monitor the printer activity duty cycle

#include <stdint.h>
#include <stdbool.h>
#include "pru.h"
#include "pruprinter.h"

// Activate below definition to let the printer make use of the temperature
// sensor that's integrated into the motor-driver H bridge. If activated the
// temperature will be monitored throughout the print job and the job will
// get cancelled if the temperature exceeds safe limits and an error will get
// reported back to the host.
#define PRINTER_USE_THERMAL_SENSOR

// Activate below definition to let the printer make use of the end-of-paper
// sensor integrated into the printer assembly. If activated and the paper runs
// out during printing the job will get cancelled and an error will get reported
// back to the host.
#define PRINTER_USE_PAPER_SENSOR

// Interface to the printer circuitry. The bits defined here map to bits in
// PRU 1 core register R30 for outputs and to bits in R31 for inputs.
#define PRINTER_OUT_PAPER_SENSE     (1 << 0)    // BB P8.45 - Powers the paper-sense circuit
#define PRINTER_OUT_STB1_N          (1 << 1)    // BB P8.46 - Strobe dots 321 to 384
#define PRINTER_OUT_STB23_N         (1 << 2)    // BB P8.43 - Strobe dots 193 to 320
#define PRINTER_OUT_STB4_N          (1 << 3)    // BB P8.44 - Strobe dots 129 to 192
#define PRINTER_OUT_STB56_N         (1 << 4)    // BB P8.41 - Strobe dots 1 to 128
#define PRINTER_OUT_CLK             (1 << 5)    // BB P8.42 - Serial clock
#define PRINTER_OUT_LAT_N           (1 << 6)    // BB P8.39 - Latch signal
#define PRINTER_OUT_MOSI            (1 << 7)    // BB P8.40 - Serial data
#define PRINTER_OUT_A1              (1 << 8)    // BB P8.27 - Motor driver A1
#define PRINTER_OUT_A2              (1 << 9)    // BB P8.29 - Motor driver A2
#define PRINTER_OUT_B1              (1 << 10)   // BB P8.28 - Motor driver B1
#define PRINTER_OUT_B2              (1 << 11)   // BB P8.30 - Motor driver B2
#define PRINTER_OUT_PWR_N           (1 << 12)   // BB P8.21 - Powers the printer head
#define PRINTER_IN_ALARM_N          (1 << 13)   // BB P8.20 - Thermal alarm
#define PRINTER_IN_PAPER_OUT        (1 << 16)   // BB P9.26 - Paper out

// Convenience macros for accessing the input/output bits in the core registers
#define PRU_OUT_SET(x)              { __R30 |= (x); }
#define PRU_OUT_CLR(x)              { __R30 &= ~(x); }
#define PRU_IN(x)                   (__R31 & (x))

// Bit-indexes of the printer line when certain strobe signals need to be
// asserted
#define STB1_BYTE_INDEX             (384 / 8 - 1)
#define STB23_BYTE_INDEX            (320 / 8 - 1)
#define STB4_BYTE_INDEX             (192 / 8 - 1)
#define STB56_BYTE_INDEX            (128 / 8 - 1)

// General PRU-timing related definitions. Note that for the delay definitions
// to work the PRU core frequency must have been defined correctly.
#define F_PRU_OCP_CLK_HZ            ((uint32_t)200E06)
#define DELAY_5_MS                  ((uint32_t)(F_PRU_OCP_CLK_HZ * 0.005))
#define DELAY_100_MS                ((uint32_t)(F_PRU_OCP_CLK_HZ * 0.100))
#define DELAY_500_MS                ((uint32_t)(F_PRU_OCP_CLK_HZ * 0.500))

// Printer communication-related timing definitions. Those come straight from
// the printer head's datasheet.
#define DELAY_TW_CLK                ((uint32_t)(F_PRU_OCP_CLK_HZ / 8E06 / 2))
#define DELAY_TSETUP_DI             ((uint32_t)(F_PRU_OCP_CLK_HZ * 70E-09))
#define DELAY_THOLD_DI              ((uint32_t)(F_PRU_OCP_CLK_HZ * 30E-09))
#define DELAY_TSETUP_LAT            ((uint32_t)(F_PRU_OCP_CLK_HZ * 300E-09))
#define DELAY_TW_LAT                ((uint32_t)(F_PRU_OCP_CLK_HZ * 200E-09))
#define DELAY_THOLD_LAT             ((uint32_t)(F_PRU_OCP_CLK_HZ * 50E-09))
#define DELAY_TSETUP_STB            ((uint32_t)(F_PRU_OCP_CLK_HZ * 300E-09))
#define DELAY_TD0                   ((uint32_t)(F_PRU_OCP_CLK_HZ * 3000E-09))
#define DELAY_TD1                   ((uint32_t)(F_PRU_OCP_CLK_HZ * 3000E-09))

// The below delay determines how long the printer dots will be energized. The
// exact value needed depends on various conditions. See printer head datasheet
// for more information.
#define DELAY_STB                   ((uint32_t)(F_PRU_OCP_CLK_HZ * 1E-03))

// The below delay determines the paper feed speed. The printer driver waits at
// least the specified time between stepper motor half-steps. The below value
// corresponds to a paper feed speed of 60mm/s. See printer head datasheet for
// more information.
#define DELAY_HALF_STEP             ((uint32_t)(F_PRU_OCP_CLK_HZ * 1.041667E-03))

// General helper macro - determine and return the maximum of two given values
#define MAX(a, b)                   (((a) > (b)) ? (a) : (b))

// Map the PRU shared RAM that is used as the printer queue to a local variable
// for easier access.
volatile far PRINTER_Queue queue __attribute__((cregister("C28_SHARED_RAM", far), peripheral));

// Keeps track of the current state of the stepper motor
static uint8_t motorStepIndex;

// Init and test functions
static void initPRU(void);
static void initIEP(void);
static void setIepCompareEvent0(const uint32_t count);
static void waitForIepCompareEvent0(void);
static void initPrinterStatusRegister(void);
static void initPrinterOutputSignals(void);
static void testPrinterOutputSignals(void);

// Functions used for printing
static void processPrintJob(const PRINTER_JobItem *job);
static void printLine(const uint8_t dotData[]);
static void printerStrobe(const uint32_t strobeSignal);

// Functions for controlling the stepper motor
static bool initMotor(void);
static bool advanceMotorHalfStep(void);
static bool checkThermalAlarm(void);

// Paper management
static bool checkPaperSensor(void);

// Program entry point and event processing loop
int main(void) {
    // Perform various PRU and printer-related initialization
    initPRU();
    initIEP();
    initPrinterStatusRegister();
    initPrinterOutputSignals();

    // Process print jobs which get started through ARM-to-PRU interrupts until
    // during processing a command to shutdown the PRU is encountered. This will
    // allow us to concatenate several print jobs if needed without disrupting
    // the state of the print process.
    while (!queue.status.bits.pruHaltRequested) {
        // Wait until receipt of interrupt from host via PRU interrupt 1 from
        // local INTC. The INTC config maps this event to channel 1 (host 1).
        // For this to work the interrupt must have been enabled by the host
        // driver as well (via ESR0 or ESR1 registers).
        while ((__R31 & 0x80000000) == 0) {
        }

        // Clear status of system interrupt 22 event (ARM_PRU1_INTERRUPT) in
        // SECR1. This will reset the associated PRU interrupt 1 flag in __R31
        // that has been associated with channel 1 (host 1).
        CT_INTC.secr0 = 1 << 22;

        // Process the print job that was submitted by the host. In case we ever
        // need double-buffering we can easily split the available PRU memory
        // into two sections and alternate between them - filling one while
        // printing the other. processPrintJob() already supports such a scheme
        // through the use of a the pointer to the first print job item.
        processPrintJob((PRINTER_JobItem *)queue.jobItems);

        // Interrupt Host for print job completion. At this point (and only
        // then!) the host can/should also read out the printer driver's
        // status register.
        __R31 = PRU1_ARM_INTERRUPT;
    }

    // Before proceeding to power down the PRU let's wait for a little bit to
    // give the host a chance to see the PRU->ARM interrupt we triggered as we
    // will be disabling all the interrupts in the next step.
    __delay_cycles(DELAY_500_MS);

    // Halt PRU core. Before that, clear all system interrupts as required to
    // allow the PRU to power down. Note that because of the call to __halt()
    // the main() function will actually never return.
    CT_INTC.secr0 = 0xffffffff;
    CT_INTC.secr1 = 0xffffffff;
    __halt();
}

static void initPRU(void) {
    // Clear SYSCFG[STANDBY_INIT] to enable OCP master port
    CT_CFG.syscfg &= ~(1 << 4);

    // Set C28_POINTER base address to 0x0001_0000 which is the beginning of the
    // PRU shared memory region. This is were our printer queue will reside.
    CTPPR0 = 0x00000100;
}

// Many of the register initializations below are redundant when coming from a
// PRU reset as those bits get initialized by hardware. Nevertheless as a good
// practice to minimize dependencies let's re-initialize them with exactly the
// configuration we need.
static void initIEP(void) {
    // Select ocp_clk as the clock source for the IEP peripheral to ensure it
    // is synchronous with the PRU core clock. This is for cycle-accuracy and
    // also to make sure that there are no race conditions as we use the PRU
    // core to access the counroott register.
    CT_CFG.iepclk |= (1 << 0);

    // Ensure counter is disabled. Set default an increment value of 1.
    CT_IEP.global_cfg = (1 << 4);

    // Disable all compare registers and counter reset on compare 0
    CT_IEP.cmp_cfg = 0;

    // Reset Count register by writing '1' to each bit
    CT_IEP.count = 0xffffffff;

    // Disable compensation
    CT_IEP.compen = 0;

    // Clear overflow status bit by writing '1' to it
    CT_IEP.global_status = (1 << 0);

    // Clear match status bit for compare blocks 0 to 7 by writing '1' to each bit
    CT_IEP.cmp_status = 0xff;

    // Enable the counter
    CT_IEP.global_cfg |= (1 << 0);
}

static void setIepCompareEvent0(const uint32_t count) {
    // Enable IEP compare register 0
    CT_IEP.cmp_cfg |= (0x01 << 1);

    // Set compare value for compare block 0. We simply add the desired value
    // value to the current counter, effectively operating the timer in
    // continuous mode which allows us to operate and use all timer blocks
    // independently should this ever be needed.
    CT_IEP.cmp0 = CT_IEP.count + count;

    // Clear match status bit for compare block 0 by writing '1' to ensure that
    // we are really waiting for the event that is going to occur.
    CT_IEP.cmp_status = 0x01;
}

static void waitForIepCompareEvent0(void) {
    // Wait for compare match to occur. It's possible that the event has
    // already occurred while we were off doing other things. In this case we
    // will return right away.
    while (!(CT_IEP.cmp_status & 0x01)) {
    }

    // Disable IEP compare register 0
    CT_IEP.cmp_cfg &= ~(0x01 << 1);
}

static void initPrinterStatusRegister(void) {
    queue.status.all = 0;
}

// Initializes all thermal printer signals to put the printer into a safe and
// unpowered mode (which should ideally have happened already through U-Boot
// or the Kernel configuration). Note that the signals with inverse logic '_N'
// are set to high.
static void initPrinterOutputSignals(void) {
    PRU_OUT_CLR(PRINTER_OUT_A1);
    PRU_OUT_CLR(PRINTER_OUT_A2);
    PRU_OUT_CLR(PRINTER_OUT_B1);
    PRU_OUT_CLR(PRINTER_OUT_B2);
    PRU_OUT_SET(PRINTER_OUT_STB1_N);
    PRU_OUT_SET(PRINTER_OUT_STB23_N);
    PRU_OUT_SET(PRINTER_OUT_STB4_N);
    PRU_OUT_SET(PRINTER_OUT_STB56_N);
    PRU_OUT_CLR(PRINTER_OUT_CLK);
    PRU_OUT_SET(PRINTER_OUT_LAT_N);
    PRU_OUT_CLR(PRINTER_OUT_MOSI);
    PRU_OUT_CLR(PRINTER_OUT_PAPER_SENSE);
    PRU_OUT_SET(PRINTER_OUT_PWR_N);
}

// Function that cycles all low-level output signals in a specific sequence.
// Note that this function is only to be used during firmware/hardware
// development/debugging. It should NOT be used if the actual printer head is
// connected, otherwise damage may occur.
static void testPrinterOutputSignals(void) {
    const uint32_t testVector[] = {
        PRINTER_OUT_A1,
        PRINTER_OUT_A2,
        PRINTER_OUT_B1,
        PRINTER_OUT_B2,
        PRINTER_OUT_STB1_N,
        PRINTER_OUT_STB23_N,
        PRINTER_OUT_STB4_N,
        PRINTER_OUT_STB56_N,
        PRINTER_OUT_CLK,
        PRINTER_OUT_LAT_N,
        PRINTER_OUT_MOSI,
        PRINTER_OUT_PAPER_SENSE,
        PRINTER_OUT_PWR_N
    };

    uint16_t i;

    while (1) {
        // Iterate through the entire test vector. Turn each output on for 0.5s
        // before turning it off again and waiting another 0.5s.
        for (i = 0; i < sizeof(testVector) / sizeof(testVector[0]); i++) {
            PRU_OUT_SET(testVector[i]);
            __delay_cycles(DELAY_500_MS);
            PRU_OUT_CLR(testVector[i]);
            __delay_cycles(DELAY_500_MS);
        }
    }
}

static void processPrintJob(const PRINTER_JobItem *job) {
    PRINTER_JobItem *currentItem = (PRINTER_JobItem *)job;
    bool endJob = false;

    while (!endJob) {
        switch (currentItem->command) {
        case PRINTER_CMD_OPEN:
            // (Re-)Initialize all printer output signals to a known-safe state.
            initPrinterOutputSignals();
            __delay_cycles(DELAY_5_MS);
            // Turn on the printer head control logic and the end-of-paper
            // sensor supply. Then, wait a predetermined amount of time for the
            // voltages to settle. This amount can likely be made much shorter
            // however let's be conservative for now until the final demo
            // hardware been designed.
            PRU_OUT_CLR(PRINTER_OUT_PWR_N);
            PRU_OUT_SET(PRINTER_OUT_PAPER_SENSE);
            __delay_cycles(DELAY_100_MS);
            // Initialize the stepper motor. In case the initialization fails we
            // are going to end the print job right away.
            if (!initMotor()) {
                endJob = true;
            }
            break;
        case PRINTER_CMD_PRINT_LINE:
            // Before printing the line do a sanity check on the supplied print
            // data to make sure is exactly as long as we expect. This safety
            // mechanism could potentially save us from printing a bunch of
            // garbage.
            if (currentItem->length == PRINTER_BYTES_PER_LINE) {
                printLine((uint8_t *)currentItem->data);
            }
            break;
        case PRINTER_CMD_MOTOR_HALF_STEP:
            // Before advancing the paper do a sanity check that the payload
            // size field denoting how far to advance has the proper size, and
            // also limit the paper advance to a reasonable default. This may
            // save us from wasting a bunch of paper under certain erroneous
            // operating conditions. Furthermore, in case we encounter an error
            // we will stop the print job.
            if (currentItem->length == sizeof(uint32_t)) {
                uint32_t numberOfHalfSteps = (currentItem->data)[0];
                if (numberOfHalfSteps <= PRINTER_MAX_NR_HALF_STEPS) {
                    uint32_t i;
                    for (i = 0; i < numberOfHalfSteps; i++) {
                        if (!advanceMotorHalfStep()) {
                            endJob = true;
                        }
                    }
                }
                else {
                    // The host tried to issue a step command with a parameter
                    // that was too large. Be a good citizen and report an error
                    // back rather than to just silently ignore the command.
                    queue.status.bits.illegalParameterError = true;
                }
            }
            break;
        case PRINTER_CMD_TEST_SIGNALS:
            // Repeatedly send out the signal test vector. Note that this
            // function will never return.
            testPrinterOutputSignals();
            break;
        case PRINTER_CMD_CLOSE:
            // Wait a short moment to prevent glitching and then turn off the
            // stepper motor completely
            __delay_cycles(DELAY_5_MS);
            initMotor();
            // Turn off the end-of-paper sensor supply and the printer head
            // control logic
            PRU_OUT_CLR(PRINTER_OUT_PAPER_SENSE);
            PRU_OUT_SET(PRINTER_OUT_PWR_N);
            break;
        case PRINTER_CMD_REQUEST_PRU_HALT:
            // The host has requested a shut-down of the PRU after this print
            // job. Set a status flag that will get processed once we are
            // through with the print job.
            queue.status.bits.pruHaltRequested = true;
            break;
        case PRINTER_CMD_EOS:
            // Exit the print job processing loop
            endJob = true;
            break;
        default:
            // We should not get here. Exit the print job processing loop.
            queue.status.bits.illegalCommandError = true;
            endJob = true;
        }

        if (!endJob) {
            // Advance to the next item in the print job. This is done by moving
            // the pointer across the static command and length fields of the
            // current print job item and then further moving it over all of its
            // associated payload (if any).
            currentItem = (PRINTER_JobItem *)((uint8_t *)currentItem +
                    2 * sizeof(uint32_t) + currentItem->length);
        }
    }
}

static void printLine(const uint8_t dotData[]) {
    uint8_t byteIndex;
    uint8_t bitValue;
    uint16_t blackDotCounter = 0;

    // Iterate through all bytes in one line
    for (byteIndex = 0; byteIndex < PRINTER_BYTES_PER_LINE; byteIndex++) {
        // Iterate through all bits in each pixel-data byte
        for (bitValue = 0x80; bitValue != 0x00; bitValue >>= 1) {
            // Set the serial data output in case the pixel is set
            if (dotData[byteIndex] & bitValue) {
                // Ensure we don't print more than the maximum number of black
                // dots allowed for a line. This is a safety precaution to
                // prevent potential excess current flow in case of program
                // errors. The MPU code should never pass us a line with more
                // black dots than what is allowed.
                if (++blackDotCounter <= PRINTER_MAX_BLACK_DOTS_PER_LINE) {
                    PRU_OUT_SET(PRINTER_OUT_MOSI);
                }
                else {
                    // We stopped outputting black dots - this is an error
                    // condition. We should never get here-- only if the host
                    // hasn't properly pre-processed and partitioned the print
                    // job data.
                    queue.status.bits.tooManyBlackDotsError = true;
                    // Ensure that no more black dots will be output
                    PRU_OUT_CLR(PRINTER_OUT_MOSI);
                }
            }
            else {
                PRU_OUT_CLR(PRINTER_OUT_MOSI);
            }

            // Wait the necessary data setup time or clock width low time -
            // whichever is greater.
            __delay_cycles(MAX(DELAY_TSETUP_DI, DELAY_TW_CLK));

            // Generate one clock pulse and wait the required hold time or
            // clock width high time - whichever is greater.
            PRU_OUT_SET(PRINTER_OUT_CLK);
            __delay_cycles(MAX(DELAY_THOLD_DI, DELAY_TW_CLK));
            PRU_OUT_CLR(PRINTER_OUT_CLK);
        }
    }

    // Toggle the latch signal to accept the serial data into the printer head
    // internal buffer.
    __delay_cycles(DELAY_TSETUP_LAT);
    PRU_OUT_CLR(PRINTER_OUT_LAT_N);
    __delay_cycles(DELAY_TW_LAT);
    PRU_OUT_SET(PRINTER_OUT_LAT_N);
    __delay_cycles(DELAY_THOLD_LAT);

    // Toggle all strobe signals, one after another. This will actually print
    // the image. There is some room for optimization here to intelligently only
    // toggle the strobe lines that have actual black dots in their associated
    // sections.
    printerStrobe(PRINTER_OUT_STB1_N);
    printerStrobe(PRINTER_OUT_STB23_N);
    printerStrobe(PRINTER_OUT_STB4_N);
    printerStrobe(PRINTER_OUT_STB56_N);
}

static void printerStrobe(const uint32_t strobeSignal) {
    // wait the setup time for the strobe signal
    __delay_cycles(DELAY_TSETUP_STB);

    // Toggle the desired strobe line and wait the associated data out delay
    // time as well as the required strobe time.
    PRU_OUT_CLR(strobeSignal);
    __delay_cycles(MAX(DELAY_TD0, DELAY_STB));
    PRU_OUT_SET(strobeSignal);

    // Wait the driver out delay time
    __delay_cycles(DELAY_TD1);
}

static bool initMotor(void) {
    PRU_OUT_CLR(PRINTER_OUT_A1 | PRINTER_OUT_A2 | PRINTER_OUT_B1 | PRINTER_OUT_B2);
    motorStepIndex = 0;

#ifdef PRINTER_USE_THERMAL_SENSOR
    if (checkThermalAlarm()) {
        queue.status.bits.thermalAlarmError = true;
        return false;
    }
#endif

    // Set initial stepper motor delay. That's important to do here since we
    // rely on this event as we enter advanceMotorHalfStep() for the first time
    // later on.
    setIepCompareEvent0(DELAY_HALF_STEP);

    return true;
}

// http://www.nmbtc.com/step-motors/engineering/full-half-and-microstepping.html
static bool advanceMotorHalfStep(void) {
    const uint32_t phaseTable[8] = {
            PRINTER_OUT_A1,
            PRINTER_OUT_A1 | PRINTER_OUT_B1,
            PRINTER_OUT_B1,
            PRINTER_OUT_B1 | PRINTER_OUT_A2,
            PRINTER_OUT_A2,
            PRINTER_OUT_A2 | PRINTER_OUT_B2,
            PRINTER_OUT_B2,
            PRINTER_OUT_B2 | PRINTER_OUT_A1
    };

#ifdef PRINTER_USE_THERMAL_SENSOR
    if (checkThermalAlarm()) {
        // Immediately turn off motor in case of any error to let the system
        // cool down.
        initMotor();
        // Report error back to the host and exit here
        queue.status.bits.thermalAlarmError = true;
        return false;
    }
#endif

    // Make sure the required time has passed since the last half step to not
    // exceed the maximum paper feed speed.
    waitForIepCompareEvent0();

    // Activate the output lines according to the next step to take. Here, we
    // chose to access the core register R30 directly (rather than using our
    // set/clear macros) so that we can perform a simultaneous set and clear
    // operation needed to avoid any output glitching.
    uint32_t r30tmp = __R30 &
        ~(PRINTER_OUT_A1 | PRINTER_OUT_A2 | PRINTER_OUT_B1 | PRINTER_OUT_B2);
    __R30 = r30tmp | phaseTable[motorStepIndex];

    // Now that the new step was taken let's set a new timer event determining
    // the minimum wait time after which the next step can be taken upon re-
    // entry into this function.
    setIepCompareEvent0(DELAY_HALF_STEP);

    // Wrap the phase table index if the end of the table has been reached
    if (++motorStepIndex >= (sizeof(phaseTable) / sizeof(phaseTable[0]))) {
        motorStepIndex = 0;
    }

#ifdef PRINTER_USE_PAPER_SENSOR
    if (checkPaperSensor()) {
        // Immediately turn off motor in case of any error. We don't want to
        // keep the windings energized when there is no paper.
        initMotor();
        queue.status.bits.paperOutError = true;
        return false;
    }
#endif

    return true;
}

static bool checkThermalAlarm(void) {
    // Read the fault pin from the motor driver chip and return true in case
    // of a thermal error condition. Note that we need to invert the result as
    // the actual signal is active-low.
    return !PRU_IN(PRINTER_IN_ALARM_N);
}

static bool checkPaperSensor(void) {
    // Check if there is still paper and return true in case the paper is out.
    // The lack of paper causes the printer-head built-in photo transistor to
    // be open and the output signal to get pulled high.
    return PRU_IN(PRINTER_IN_PAPER_OUT);
}
