/*
 * linker.cmd
 *
 * Linker command file that can be used for linking programs built with the PRU
 * C Compiler in conjunction with CCSv6. Note that this linker command file is
 * supposed to be used with the RAM autoinitialization model (--ram_model) so
 * make sure to set this up in CCS accordingly. Also set -stack and -heap sizes
 * as needed.
 *
 * Written by Andreas Dannenberg, 01/01/2014
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 * ALL RIGHTS RESERVED
 */

/* ADDITIONAL LINKER OPTIONS */

/* --ram_model */
/* --stack_size 0x100 */
/* --heap_size 0x100 */

/*
 * SPECIFY THE SYSTEM MEMORY MAP
 *
 * Note that we place the cregister definitions into their own page so that
 * they can overlap any code and data memory definitions that are made. This
 * is okay since everything allocated in this section is declared with the
 * 'cregister' and 'peripheral' attributes and therefore does not really place
 * any data into those sections and with that doesn't interfere with the actual
 * use of that memory by the compiler.
 */

MEMORY
{
    PAGE 0:
      PRUIMEM   : org = 0x00000000, len = 0x00002000  /* 8KB PRU Instruction RAM */
    PAGE 1:
      PRUDMEM   : org = 0x00000000, len = 0x00002000  /* 8KB PRU Data RAM */
      SHAREDMEM : org = 0x00010000, len = 0x00003000  /* 12KB Shared RAM */
    PAGE 2:
      C0_INTC   : org = 0x00020000, len = 0x00001504, cregister = 0
      MEMC1     : org = 0x48040000, len = 0x00000100, cregister = 1
      MEMC2     : org = 0x4802A000, len = 0x00000100, cregister = 2
      MEMC3     : org = 0x00030000, len = 0x00000100, cregister = 3
      C4_CFG    : org = 0x00026000, len = 0x00000100, cregister = 4
      MEMC5     : org = 0x48060000, len = 0x00000100, cregister = 5
      MEMC6     : org = 0x48030000, len = 0x00000100, cregister = 6
      MEMC7     : org = 0x00028000, len = 0x00000100, cregister = 7
      MEMC8     : org = 0x46000000, len = 0x00000100, cregister = 8
      MEMC9     : org = 0x4A100000, len = 0x00000100, cregister = 9
      MEMC10    : org = 0x48318000, len = 0x00000100, cregister = 10
      MEMC11    : org = 0x48022000, len = 0x00000100, cregister = 11
      MEMC12    : org = 0x48024000, len = 0x00000100, cregister = 12
      MEMC13    : org = 0x48310000, len = 0x00000100, cregister = 13
      MEMC14    : org = 0x481CC000, len = 0x00000100, cregister = 14
      MEMC15    : org = 0x481D0000, len = 0x00000100, cregister = 15
      MEMC16    : org = 0x481A0000, len = 0x00000100, cregister = 16
      MEMC17    : org = 0x4819C000, len = 0x00000100, cregister = 17
      MEMC18    : org = 0x48300000, len = 0x00000100, cregister = 18
      MEMC19    : org = 0x48302000, len = 0x00000100, cregister = 19
      MEMC20    : org = 0x48304000, len = 0x00000100, cregister = 20
      MEMC21    : org = 0x00032400, len = 0x00000100, cregister = 21
      MEMC22    : org = 0x480C8000, len = 0x00000100, cregister = 22
      MEMC23    : org = 0x480CA000, len = 0x00000100, cregister = 23

/*
 * Note that constant table registers C24 to C30 actual value depends
 * on a base address that needs to be configured as well in the PRU
 * control register map.
 */

/*
      MEMC24    : org = 0x00000000, len = 0x00000000, cregister = 24
      MEMC25    : org = 0x00000000, len = 0x00000000, cregister = 25
*/
      C26_IEP   : org = 0x0002E000, len = 0x00000100, cregister = 26
/*
      MEMC27    : org = 0x00000000, len = 0x00000000, cregister = 27
*/
      C28_SHARED_RAM : org = 0x00010000, len = 0x00003000, cregister = 28
/*
      MEMC29    : org = 0x00000000, len = 0x00000000, cregister = 29
      MEMC30    : org = 0x00000000, len = 0x00000000, cregister = 30
*/
      C31_DDR   : org = 0x80000000, len = 0x00000100, cregister = 31
}

/* SPECIFY THE SECTIONS ALLOCATION INTO MEMORY */

SECTIONS
{
    /*
     * Force startup code to address zero which is where the PRU driver sets
     * the program counter to. For this, use wildcards so that this works even
     * as specialized boot routines are used.
     */
    .text:_c_int00 { *(.text:_c_int00*) } load = 0x0000, page = 0

    /* Executable Code */
    .text           : load = PRUIMEM, page = 0

    /* Various Data Sections */
    .stack          : load = PRUDMEM, page = 1, fill = 0x00 /* Stack space (size is controlled by --stack_size option) initialized with zero to make it easier to analyze during debugging */
    .bss            : load = PRUDMEM, page = 1      /* Uninitialized near data */
    .data           : load = PRUDMEM, page = 1, palign = 2  /* Initialized near data */
    .rodata         : load = PRUDMEM, page = 1      /* Constant read only near data */
    .init_array     : load = PRUDMEM, page = 1      /* Table of constructors to be called at startup */
    .sysmem         : load = PRUDMEM, page = 1      /* Heap for dynamic memory allocation (size is controlled by --heap_size option) */
    .cinit          : load = PRUDMEM, page = 1      /* Tables for initializing global data at runtime */
    .args           : load = PRUDMEM, page = 1      /* Section for passing arguments in the argv array to main */
    .farbss         : load = SHAREDMEM, page = 1    /* Uninitialized far data */
    .fardata        : load = SHAREDMEM, page = 1    /* Initialized far data */
    .rofardata      : load = SHAREDMEM, page = 1    /* Constant read only far data */
}
