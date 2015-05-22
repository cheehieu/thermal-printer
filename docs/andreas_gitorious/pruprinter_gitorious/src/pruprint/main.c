/*
 * main.c
 *
 * AM335x PRU-based Thermal Printer Demo Application
 *
 * This is a command line program that takes a PNG image, processes and slices
 * it into a print job suitable for the associated low-level PRU thermal printer
 * firmware, and then kicks off the printing of the image. The program also
 * offers various test and control functions that can be invoked by using
 * command line options. See the program's usage info for additional info.
 *
 * Written by Andreas Dannenberg, 01/01/2014
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 * ALL RIGHTS RESERVED
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include <png.h>

// PRU driver header file
#include "prussdrv.h"
#include "pruss_intc_mapping.h"

// Interface to the PRU-based printer driver firmware "pruprinter_fw"
#include "pruprinter.h"

// Include the generated PRU firmware from the "pruprinter_fw" project by
// including the associated header files.
#include "pruprinter_fw_iram.h"
#include "pruprinter_fw_dram.h"

#define USAGE_STRING                                                    \
    "Usage: %s [OPTION]... FILE\n"                                      \
    "       %s -f COUNT\n"                                              \
    "       %s -t\n"                                                    \
    "Prints the PNG image FILE using the PRU printer\n"                 \
    "\n"                                                                \
    "  -s START     First image row to print\n"                         \
    "  -e END       Last image row to print\n"                          \
    "  -i           Invert image while printing\n"                      \
    "  -f COUNT     Feed printer paper\n"                               \
    "  -t           Test pattern signal generation\n"                   \
    "               CAUTION: USE ONLY WITH NO PRINTER HW CONNECTED!\n"  \
    "  -w           Wait for ENTER before disabling PRU and exiting program\n"

// Global variable pointing to the printer queue that is located in the PRU
// shared memory section
static PRINTER_Queue *queue;

// Global variable pointing to the next job item in the printer queue that needs
// to be processed.
static PRINTER_JobItem *jobItem;

// Global variable pointing to the last valid memory location for job items
static void *jobItemsMaxAddress;

// Global variables for working with the image that was loaded
static uint32_t pngImageWidth, pngImageHeight;
static png_bytep *pngImageRowPointers;

// Function prototypes
static bool initPru(void);
static void disablePru(void);
static bool readPngImage(const char *fileName);
static void deallocPngImage(void);
static void initQueueJobItems(void);
static bool queueHasJobItems(void);
static void addJobItemToQueue(const uint32_t command, const uint32_t length,
        const uint8_t data[]);
static bool addJobItemToQueueLowLevel(const uint32_t command,
        const uint32_t length, const uint8_t data[]);
static void printImage(const uint32_t startLine, const uint32_t endLine,
        const bool inverse, const uint32_t paperFeedCountAfterPrint);
static void partitionLineAndPrint(const uint8_t dotData[],
        const uint16_t length, const bool inverse);
void measureDurationPrintToConsole(bool start);
void checkForPrinterErrorsPrintToConsole(void);

// Main Linux program entry point
int main(int argc, char *argv[]) {
    int opt;
    bool testFlag = false;
    bool paperFeedFlag = false;
    uint32_t paperFeedCount = 0;
    bool startLineFlag = false;
    uint32_t startLine = 0;
    bool endLineFlag = false;
    uint32_t endLine = 0;
    bool inverseFlag = false;
    bool waitFlag = false;

    // Parse the command line options and issue a simple help text in case
    // things don't match up. The columns behind the options denote that option
    // requires an argument. See getopt(3) for more info.
    while ((opt = getopt(argc, argv, "tf:s:e:iw")) != -1) {
        switch (opt) {
        case 't':
            testFlag = true;
            break;
        case 'f':
            paperFeedCount = atoi(optarg);
            paperFeedFlag = true;
            break;
        case 's':
            startLine = atoi(optarg);
            startLineFlag = true;
            break;
        case 'e':
            endLine = atoi(optarg);
            endLineFlag = true;
            break;
        case 'i':
            inverseFlag = true;
            break;
        case 'w':
            waitFlag = true;
            break;
        default:
            // getopt() will return '?' in case of a malformed command line in
            // which case we are printing the usage and exit the command.
            fprintf(stderr, USAGE_STRING, argv[0], argv[0], argv[0]);
            return EXIT_FAILURE;
        }
    }

    // Initialize the PRU and exit the program if that fails. Any errors that
    // may occur during that process will be output from within that function.
    if (!initPru()) {
        return EXIT_FAILURE;
    }

    // See if the test mode has been activated. If that's the case we will just
    // enter test mode right away.
    if (testFlag) {
        // Create a very simple print job that activates the test pattern
        // generation. Since this sub-function doesn't return within the PRU
        // firmware we don't need to bother trying to issue a halt command.
        initQueueJobItems();
        addJobItemToQueue(PRINTER_CMD_TEST_SIGNALS, 0, NULL);
        addJobItemToQueue(PRINTER_CMD_EOS, 0, NULL);

        // The interrupt is mapped via INTC to channel 1
        printf("Starting PRU GPIO test pattern generation\n");
        prussdrv_pru_send_event(ARM_PRU1_INTERRUPT);
    }
    // See if the paper feed flag has been set AND no image filename was given.
    // Unlike other print-related flags we want to allow the user to feed paper
    // without needing to specify an image to print.
    else if (paperFeedFlag && (optind >= argc)) {
        // Go ahead and create a very simple print job that simply feeds the
        // paper by the specified number of steps. Any other print-related
        // command line option will be ignored.
        initQueueJobItems();
        addJobItemToQueue(PRINTER_CMD_OPEN, 0, NULL);
        addJobItemToQueue(PRINTER_CMD_MOTOR_HALF_STEP, sizeof(uint32_t),
                (const uint8_t *)&paperFeedCount);
        addJobItemToQueue(PRINTER_CMD_CLOSE, 0, NULL);
        addJobItemToQueue(PRINTER_CMD_REQUEST_PRU_HALT, 0, NULL);
        addJobItemToQueue(PRINTER_CMD_EOS, 0, NULL);

        // The interrupt is mapped via INTC to channel 1
        printf("Start feeding paper\n");
        prussdrv_pru_send_event(ARM_PRU1_INTERRUPT);

        // Wait until PRU1 has finished execution and acknowledge the interrupt.
        // The INTC config maps PRU1_ARM_INTERRUPT to EVTOUT_1.
        printf("Waiting for paper feed completion...\n");
        measureDurationPrintToConsole(true);
        prussdrv_pru_wait_event(PRU_EVTOUT_1);
        measureDurationPrintToConsole(false);
        prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);

        // See if any errors occurred and output them to the console if any
        checkForPrinterErrorsPrintToConsole();
    }
    // See if we are in the normal printer operating mode which means the user
    // has provided an image filename parameter.
    else if (optind < argc) {
        // Let's go ahead and print the image considering any of the other
        // command line flags that may have been set.
        const char *imageFile = argv[optind];

        printf("Loading image %s\n", imageFile);
        if (!readPngImage(imageFile)) {
            return EXIT_FAILURE;
        }

        // Check if a start line was given and use it if it is a valid
        // parameter. Otherwise use the first line of the image.
        if (startLineFlag) {
            if ((startLine < 0) || (startLine >= pngImageHeight)) {
                fprintf(stderr, "Invalid start line!\n");
                return EXIT_FAILURE;
            }
        }
        else {
            startLine = 0;
        }

        // Check if an end line was given and use it if it is a valid
        // parameter. Otherwise use the last line of the image.
        if (endLineFlag) {
            if ((endLine < 0) || (endLine >= pngImageHeight)) {
                fprintf(stderr, "Invalid end line!\n");
                return EXIT_FAILURE;
            }
        }
        else {
            endLine = pngImageHeight - 1;
        }

        // Make sure the parameters actually make sense
        if (startLine > endLine) {
            fprintf(stderr, "The start line must not be larger than the end" \
                    " line!\n");
            return EXIT_FAILURE;
        }

        // Check the width of the image. If it's too wide we'll continue with
        // printing anyways. We just won't output the full line.
        if (pngImageWidth > PRINTER_DOTS_PER_LINE) {
            printf("Image width exceeds the maximum number of dots allowed" \
                    " per line! Will only be printing the first %u pixels...",
                    PRINTER_DOTS_PER_LINE);
        }

        printf("Processing image, transferring into PRU shared memory, and " \
                "starting print job\n");
        printImage(startLine, endLine, inverseFlag, paperFeedCount);

        // Free the PNG image from memory. It's no longer needed-- all relevant
        // data was transferred into the PRU shared memory.
        deallocPngImage();

        // See if any errors occurred and output them to the console if any
        checkForPrinterErrorsPrintToConsole();
    }
    // Looks like no command line parameters or an invalid combination thereof
    // was encountered...
    else {
        // Print the usage info to the console and exit with error
        fprintf(stderr, USAGE_STRING, argv[0], argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    if (waitFlag) {
        printf("Press ENTER to disable the PRU and end the program...\n");
        getchar();
    }

    disablePru();

    return EXIT_SUCCESS;
}

static bool initPru(void) {
    tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;

    printf("Initializing PRU\n");
    prussdrv_init();

    // Open PRU driver and prepare for using the interrupt on event output 1
    if (prussdrv_open(PRU_EVTOUT_1)) {
        fprintf(stderr, "prussdrv_open failed!\n");
        return false;
    }

    // Initialize the PRUSS interrupt controller
    if (prussdrv_pruintc_init(&pruss_intc_initdata)) {
        fprintf(stderr, "prussdrv_pruintc_init failed!\n");
        return false;
    }

    // Get pointer to the shared PRUSS memory. On the AM355x this block is 12KB
    // in size and located locally at 0x0001_0000 within the PRU cores and
    // globally at 0x4A31_0000 in the MPU's memory map. The entire memory is
    // used as our printer queue so we map the global variable to that address.
    prussdrv_map_prumem(PRUSS0_SHARED_DATARAM, (void *)&queue);

    // Initialize the PRU from an array in memory rather than from a file on
    // disk. Make sure PRU sub system is first disabled/reset. Then, transfer
    // the program into the PRU. Note that the write memory functions expect
    // the offsets to be provided in words so we our byte-addresses by four.
    printf("Loading PRU firmware and enabling PRU\n");
    prussdrv_pru_disable(1);
    prussdrv_pru_write_memory(PRUSS0_PRU1_IRAM, pruprinter_fw_iram_start / 4,
            (unsigned int *)&pruprinter_fw_iram, pruprinter_fw_iram_length);
    prussdrv_pru_write_memory(PRUSS0_PRU1_DATARAM, pruprinter_fw_dram_start / 4,
            (unsigned int *)&pruprinter_fw_dram, pruprinter_fw_dram_length);
    prussdrv_pru_enable(1);

    return true;
}

static void disablePru(void) {
    printf("Disabling PRU and closing memory mapping\n");
    prussdrv_pru_disable(1);
    prussdrv_exit();
}

static bool readPngImage(const char *fileName) {
    FILE *fp;
    unsigned char pngSignature[8];      // The PNG signature is 8 bytes long
    png_byte bitDepth;
    png_structp png_ptr;
    png_infop info_ptr;
    uint32_t y;

    // Open image file
    fp = fopen(fileName, "rb");
    if (!fp) {
        fprintf(stderr, "File could not be opened for reading!\n");
        return false;
    }

    // Test image file for being a PNG by evaluating its header
    fread(pngSignature, 1, sizeof(pngSignature), fp);
    if (png_sig_cmp(pngSignature, 0, sizeof(pngSignature))) {
        fprintf(stderr, "File not recognized as a PNG file!\n");
        return false;
    }

    // Initialize libpng in preparation for reading the image
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fprintf(stderr, "Error during during PNG initialization!\n");
        return false;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "Error during during PNG initialization!\n");
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during during PNG initialization!\n");
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    // Read important image parameters and store them in local/global variables
    pngImageWidth = png_get_image_width(png_ptr, info_ptr);
    pngImageHeight = png_get_image_height(png_ptr, info_ptr);
    bitDepth = png_get_bit_depth(png_ptr, info_ptr);

    printf("Image width = %u\n", pngImageWidth);
    printf("Image height = %u\n", pngImageHeight);

    if (bitDepth != 1) {
        fprintf(stderr, "Only monochrome images (1-bit) are allowed! Provided" \
                " image is %u bits deep.\n", bitDepth);
        return false;
    }

    // Enable the interlace handling and updates the structure pointed to by
    // info_ptr to reflect any transformations that have been requested.
    png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    // Allocate memory to hold an array of pointers that point to the respective
    // row image data
    if (pngImageRowPointers) {
        fprintf(stderr, "Error allocating memory for image. Was the last " \
                "image loaded deallocated properly?\n");
        return false;
    }
    pngImageRowPointers = (png_bytep *)malloc(sizeof(png_bytep) * pngImageHeight);
    if (!pngImageRowPointers) {
        fprintf(stderr, "Error allocating memory for image!\n");
        return false;
    }

    // Allocate an individual block of memory for each row of the image. Note that
    // this function assumes a 1 bit-per-pixel image. If an image is to be read
    // with greater color depth the amount of memory that is allocated needs to
    // be increased.
    for (y = 0; y < pngImageHeight; y++) {
        if (pngImageRowPointers[y]) {
            fprintf(stderr, "Error allocating memory for image. Was the last " \
                    "image loaded deallocated properly?\n");
            return false;
        }
        pngImageRowPointers[y] = (png_byte *)malloc((pngImageWidth + 7) / 8);
        if (!pngImageRowPointers[y]) {
            fprintf(stderr, "Error allocating memory for image!\n");
            return false;
        }
    }

    // Establish an error handler for issues during the upcoming file operation
    if (setjmp(png_jmpbuf(png_ptr))) {
           fprintf(stderr, "Error during png_read_image!\n");
           return false;
    }

    // Read the entire PNG image into memory
    png_read_image(png_ptr, pngImageRowPointers);

    fclose(fp);

    printf("Image loaded successfully\n");

    return true;
}

static void deallocPngImage(void) {
    uint32_t y;

    // Free the memory used for each line of image data
    for (y = 0; y < pngImageHeight; y++) {
        free(pngImageRowPointers[y]);
        pngImageRowPointers[y] = NULL;
    }

    // Free the memory used for the array that holds the row pointers
    free(pngImageRowPointers);
    pngImageRowPointers = NULL;
}

static void initQueueJobItems(void) {
    // Initialize the job item pointer to point to the beginning of the printer
    // queue. Also initialize that very first item to safe defaults for good
    // measure.
    jobItem = (PRINTER_JobItem *)&queue->jobItems;
    jobItem->command = PRINTER_CMD_EOS;
    jobItem->length = 0;

    // Determine the maximum possible memory location of the area that is
    // reserved to hold print job data.
    jobItemsMaxAddress =
            (uint8_t *)&queue->jobItems + sizeof(queue->jobItems) - 1;
}

static bool queueHasJobItems(void) {
    // Check if there are any job items in the printer queue and return true if
    // that's the case.
    return jobItem != (PRINTER_JobItem *)&queue->jobItems;
}

static void addJobItemToQueue(const uint32_t command, const uint32_t length,
        const uint8_t data[]) {
    // Add the currently requested command to the queue. If this fails (and it
    // can in case the memory is full) then we print what's currently in the
    // queue, re-initialize the queue, and try again.
    while (!addJobItemToQueueLowLevel(command, length, data)) {
        // The interrupt is mapped via INTC to channel 1
        printf("Initiating section printing\n");
        prussdrv_pru_send_event(ARM_PRU1_INTERRUPT);

        // Wait until PRU1 has finished execution and acknowledge the interrupt.
        // The INTC config maps PRU1_ARM_INTERRUPT to EVTOUT_1.
        printf("Waiting for printer driver...\n");
        measureDurationPrintToConsole(true);
        prussdrv_pru_wait_event(PRU_EVTOUT_1);
        measureDurationPrintToConsole(false);
        prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);

        // Initialize printer job item queue to be ready to be filled again
        initQueueJobItems();
    }
}

static bool addJobItemToQueueLowLevel(const uint32_t command,
        const uint32_t length, const uint8_t data[]) {
    // Determine where the job item pointer will be after we were to add the
    // given command as requested.
    PRINTER_JobItem *jobItemNext = (PRINTER_JobItem *)
            ((uint8_t *)jobItem + 2 * sizeof(uint32_t) + length);

    // Check and see if we have enough free memory to add the currently
    // requested command and its associated payload (if any). Also make sure
    // there is always enough space for a final end-of-sequence command. If
    // the memory is full then go ahead and append an end-of-sequence command
    // so that the low-pevel PRU printer firmware knows that it needs to stop
    // parsing the job item queue.
    if (jobItemNext > (PRINTER_JobItem *)
            ((uint8_t *)jobItemsMaxAddress - 2 * sizeof(uint32_t) + 1)) {
        jobItem->command = PRINTER_CMD_EOS;
        jobItem->length = 0;
        return false;
    }

    // Write the printer commmand and the payload length into the job queue
    jobItem->command = command;
    jobItem->length = length;

    // Transfer payload data if any, otherwise leave the data field alone
    if (length) {
        memcpy(jobItem->data, data, length);
    }

    // Advance the job item pointer to the next free memory location
    jobItem = jobItemNext;

    return true;
}

static void printImage(const uint32_t startLine, const uint32_t endLine,
        const bool inverse, const uint32_t paperFeedCountAfterPrint) {
    uint32_t y;

    // Initialize the printer queue and add the command to perform the low-level
    // initializations needed before we can start printing.
    initQueueJobItems();
    addJobItemToQueue(PRINTER_CMD_OPEN, 0, NULL);

    // Generate the print job and fill the printer queue line by line
    for (y = startLine; y < endLine; y++) {
        partitionLineAndPrint(pngImageRowPointers[y], pngImageWidth, inverse);
    }

    // See if a paper feed after printing was requested and add it to the queue
    if (paperFeedCountAfterPrint) {
        addJobItemToQueue(PRINTER_CMD_MOTOR_HALF_STEP, sizeof(uint32_t),
                (const uint8_t *)&paperFeedCountAfterPrint);
    }

    // Close out the print job properly including a shutdown of the PRU that
    // is no longer needed.
    addJobItemToQueue(PRINTER_CMD_CLOSE, 0, NULL);
    addJobItemToQueue(PRINTER_CMD_REQUEST_PRU_HALT, 0, NULL);
    addJobItemToQueue(PRINTER_CMD_EOS, 0, NULL);

    // See if there are still job items in the queue and print them if that's
    // the case (which is most likely).
    if (queueHasJobItems()) {
        // The interrupt is mapped via INTC to channel 1
        printf("Initiating section printing\n");
        prussdrv_pru_send_event(ARM_PRU1_INTERRUPT);

        // Wait until PRU1 has finished execution and acknowledge the interrupt.
        // The INTC config maps PRU1_ARM_INTERRUPT to EVTOUT_1.
        printf("Waiting for printer driver...\n");
        measureDurationPrintToConsole(true);
        prussdrv_pru_wait_event(PRU_EVTOUT_1);
        measureDurationPrintToConsole(false);
        prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);
    }
}

// TODO: Balance number of black dots per line if line needs to be partitioned
static void partitionLineAndPrint(const uint8_t dotData[],
        const uint16_t length, const bool inverse) {
    uint8_t byteIndex;
    uint16_t bitIndex;
    uint8_t bitValue;
    bool dotValue;
    uint16_t blackDotCounter = 0;
    uint8_t dotBuffer[PRINTER_BYTES_PER_LINE];

    // Clear out the line of of data we are about to print so that we can
    // properly print images which are smaller than PRINTER_BYTES_PER_LINE
    // without any random garbage getting added.
    memset(&dotBuffer, 0, sizeof(dotBuffer));

    // Iterate through all bytes in one line
    for (byteIndex = 0, bitIndex = 0; byteIndex < PRINTER_BYTES_PER_LINE;
            byteIndex++) {
        // Iterate through all bits in each pixel-data byte while counting
        // each dot that is being processed
        for (bitValue = 0x80; (bitValue != 0x00) && (bitIndex < length);
                bitValue >>= 1, bitIndex++) {
            // Check for a set dot in the source data and invert if needed
            dotValue = dotData[byteIndex] & bitValue;
            dotValue ^= inverse;
            // If resulting dot is set then copy source bits to the output
            // buffer bit-by-bit.
            if (dotValue) {
                dotBuffer[byteIndex] |= bitValue;
                blackDotCounter++;
                // If we reach the maximum number of black dots allowed per
                // line we will print out the actual line, clear the buffer, and
                // reset the counter and continue to accumulate black dots to be
                // printed out in the next line that is output (which will get
                // printed into the same physical line).
                if (blackDotCounter >= PRINTER_MAX_BLACK_DOTS_PER_LINE) {
                    addJobItemToQueue(PRINTER_CMD_PRINT_LINE,
                            PRINTER_BYTES_PER_LINE, (const uint8_t *)dotBuffer);
                    memset(&dotBuffer, 0, sizeof(dotBuffer));
                    blackDotCounter = 0;
                }
            }
        }
    }

    // Check if there are any black dots that haven't been printed yet (which
    // will most likely be the case), and if so go ahead and print them.
    if (blackDotCounter) {
        addJobItemToQueue(PRINTER_CMD_PRINT_LINE, PRINTER_BYTES_PER_LINE,
                (const uint8_t *)dotBuffer);
    }

    // After all dots have been output its finally time to issue a command to
    // advance the stepper motor to the next physical line.
    const uint32_t nrOfHalfSteps = 1;
    addJobItemToQueue(PRINTER_CMD_MOTOR_HALF_STEP, sizeof(uint32_t),
            (const uint8_t *)&nrOfHalfSteps);
}

void measureDurationPrintToConsole(bool start) {
    static struct timeval startTime;
    struct timeval endTime;
    __time_t seconds;
    __suseconds_t useconds;
    uint64_t mseconds;

    // When the start flag is set the function captures the current system time.
    // When called again with the start flag cleared it will output the time
    // difference from the last call to the console.
    if (start) {
        gettimeofday(&startTime, NULL);
    }
    else {
        gettimeofday(&endTime, NULL);
        seconds = endTime.tv_sec - startTime.tv_sec;
        useconds = endTime.tv_usec - startTime.tv_usec;
        mseconds = (uint64_t)seconds * 1000 + useconds / 1000;
        printf("Elapsed time: %llu ms\n", mseconds);
    }
}

void checkForPrinterErrorsPrintToConsole(void) {
    // Create a variable to keep track if any error occurred. Then, go ahead
    // and look at the various printer status bits one by one.
    bool errorOccured = false;

    if (queue->status.bits.illegalCommandError) {
        fprintf(stderr, "Illegal command error occurred!\n");
        errorOccured = true;
    }

    if (queue->status.bits.illegalParameterError) {
        fprintf(stderr, "Illegal parameter error occurred!\n");
        errorOccured = true;
    }

    if (queue->status.bits.paperOutError) {
        fprintf(stderr, "Paper out error occurred!\n");
        errorOccured = true;
    }

    if (queue->status.bits.thermalAlarmError) {
        fprintf(stderr, "Thermal alarm error occurred!\n");
        errorOccured = true;
    }

    if (!errorOccured) {
        printf("Job completed successfully\n");
    }
}
