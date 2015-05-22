/*
 * pruprinter.h
 *
 * Header file to access the PRU-based printer driver from the host application
 *
 * Written by Andreas Dannenberg, 01/01/2014
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 * ALL RIGHTS RESERVED
 */

#ifndef PRUPRINTER_H_
#define PRUPRINTER_H_

#include <stdint.h>

// Commands to be used to stick into the 'PRINTER_JobItem.command' fields of the
// print job items.
#define PRINTER_CMD_OPEN                    0x01
#define PRINTER_CMD_PRINT_LINE              0x02
#define PRINTER_CMD_MOTOR_HALF_STEP         0x03
#define PRINTER_CMD_TEST_SIGNALS            0x04
#define PRINTER_CMD_CLOSE                   0x05
#define PRINTER_CMD_REQUEST_PRU_HALT        0xFE
#define PRINTER_CMD_EOS                     0xFF

// Define how long an actual printed line is
#define PRINTER_DOTS_PER_LINE               384
#define PRINTER_BYTES_PER_LINE              (PRINTER_DOTS_PER_LINE / 8)

// This parameter is defined by the maximum current allowed for driving the
// dots. See printer head datasheet for details.
#define PRINTER_MAX_BLACK_DOTS_PER_LINE     64

// This parameter limits how many half-steps we can advance the printer motor
// when using the PRINTER_CMD_MOTOR_HALF_STEP command.
#define PRINTER_MAX_NR_HALF_STEPS           1000

// This parameter denotes the maximum amount of job data we can store. It is
// derived from the size of the PRU memory we dedicate to our print queue (the
// PRU shared memory which is 12KB in size) less the amount of of memory used
// to keep the printer status.
#define PRINTER_MAX_JOB_SIZE                (12 * 1024 - sizeof(PRINTER_Status))

// Type containing the current status of the printer so that it can be read by
// the host processor. It is mapped to the PRU shared memory that is used as
// the printer queue.
typedef union {
    struct {
        uint32_t pruHaltRequested:1;
        uint32_t illegalCommandError:1;
        uint32_t illegalParameterError:1;
        uint32_t tooManyBlackDotsError:1;
        uint32_t thermalAlarmError:1;
        uint32_t paperOutError:1;
    } bits;
    uint32_t all;
} PRINTER_Status;

// Type containing a single job item. A print job consists of a series of job
// items. A job item's command field comprises the specific action to take.
// the length field denotes how much payload data is associated with a the
// command, and data is a variable-length array containing the actual payload
// data. Note that when length is zero no payload data is contained in the job
// item in which case the next job item will start right where the first data
// element of the previous job item would have been. If length is one then
// there is one 32-bit word of data, and so on.
typedef struct {
    uint32_t command;
    uint32_t length;
    uint32_t data[];
} PRINTER_JobItem;

// Type that describes the overarching print job queue. It will get mapped to
// the beginning of the PRU shared memory and will use as much of that memory
// as possible for storage (up to the combined size of the status register and
// PRINTER_MAX_JOB_SIZE). Note the actual type of each printer job item is
// PRINTER_JobItem but we are not using this here in this declaration since each
// item's size varies. Instead, we use uint32_t to maintain flexibility while
// ensuring alignment.
typedef struct {
    PRINTER_Status status;
    uint32_t jobItems[PRINTER_MAX_JOB_SIZE / 4];
} PRINTER_Queue;

#endif /* PRUPRINTER_H_ */
