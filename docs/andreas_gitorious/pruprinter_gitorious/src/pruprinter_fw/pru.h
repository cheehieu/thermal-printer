/*
 * pru.h
 *
 * Header file to facilitate access to important AM335x PRU control registers
 *
 * Written by Andreas Dannenberg, 01/01/2014
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 * ALL RIGHTS RESERVED
 */

#ifndef PRU_H_
#define PRU_H_

/*
 * This is the interrupt we have designated for the PRU1-to-ARM event. It
 * correlates to system interrupt 20 (PRU-generated vector #4) + 32 to ensure
 * that the strobe bit is set to push the vector out.
 */
#define PRU1_ARM_INTERRUPT      (20 - 16 + 32)

/* PRU constant table programmable pointer register 0 */
#define CTPPR0                  (*(volatile uint32_t *)(0x00024000 + 0x28))

/* PRU INTC register set */
typedef struct {
    uint32_t revid;     // 0x0
    uint32_t cr;        // 0x4
    uint32_t rsvd8[2];  // 0x8 - 0xC
    uint32_t ger;       // 0x10
    uint32_t rsvd14[2]; // 0x14 - 0x18
    uint32_t gnlr;      // 0x1C
    uint32_t sisr;      // 0x20
    uint32_t sicr;      // 0x24
    uint32_t eisr;      // 0x28
    uint32_t eicr;      // 0x2C
    uint32_t rsvd30;    // 0x30
    uint32_t hieisr;    // 0x34
    uint32_t hidisr;    // 0x38
    uint32_t rsvd3C[17];    // 0x3C - 0x7C
    uint32_t gpir;      // 0x80
    uint32_t rsvd84[95];    // 0x84 - 0x1FC
    uint32_t srsr0;     // 0x200
    uint32_t srsr1;     // 0x204
    uint32_t rsvd208[30];   // 0x208 - 0x27C
    uint32_t secr0;     // 0x280
    uint32_t secr1;     // 0x284
    uint32_t rsvd288[30];   // 0x288 - 0x2FC
    uint32_t esr0;      // 0x300
    uint32_t esr1;      // 0x304
    uint32_t rsvd308[30];   // 0x308 - 0x37C
    uint32_t ecr0;      // 0x380
    uint32_t ecr1;      // 0x384
    uint32_t rsvd388[30];   // 0x388 - 0x3FC
    uint32_t cmr0;      // 0x400
    uint32_t cmr1;      // 0x404
    uint32_t cmr2;      // 0x408
    uint32_t cmr3;      // 0x40C
    uint32_t cmr4;      // 0x410
    uint32_t cmr5;      // 0x414
    uint32_t cmr6;      // 0x418
    uint32_t cmr7;      // 0x41C
    uint32_t cmr8;      // 0x420
    uint32_t cmr9;      // 0x424
    uint32_t cmr10;     // 0x428
    uint32_t cmr11;     // 0x42C
    uint32_t cmr12;     // 0x430
    uint32_t cmr13;     // 0x434
    uint32_t cmr14;     // 0x438
    uint32_t cmr15;     // 0x43C
    uint32_t rsvd440[240];  // 0x440 - 0x7FC
    uint32_t hmr0;      // 0x800
    uint32_t hmr1;      // 0x804
    uint32_t hmr2;      // 0x808
    uint32_t rsvd80C[61];   // 0x80C - 0x8FC
    uint32_t hipir0;    // 0x900
    uint32_t hipir1;    // 0x904
    uint32_t hipir2;    // 0x908
    uint32_t hipir3;    // 0x90C
    uint32_t hipir4;    // 0x910
    uint32_t hipir5;    // 0x914
    uint32_t hipir6;    // 0x918
    uint32_t hipir7;    // 0x91C
    uint32_t hipir8;    // 0x920
    uint32_t hipir9;    // 0x924
    uint32_t rsvd928[246];  // 0x928 - 0xCFC
    uint32_t sipr0;     // 0xD00
    uint32_t sipr1;     // 0xD04
    uint32_t rsvdD08[30];   // 0xD08 - 0xD7C
    uint32_t sitr0;     // 0xD80
    uint32_t sitr1;     // 0xD84
    uint32_t rsvdD84[222];  // 0xD88 - 0x10FC
    uint32_t hinlr0;    // 0x1100
    uint32_t hinlr1;    // 0x1104
    uint32_t hinlr2;    // 0x1108
    uint32_t hinlr3;    // 0x110C
    uint32_t hinlr4;    // 0x1110
    uint32_t hinlr5;    // 0x1114
    uint32_t hinlr6;    // 0x1118
    uint32_t hinlr7;    // 0x111C
    uint32_t hinlr8;    // 0x1120
    uint32_t hinlr9;    // 0x1124
    uint32_t rsvd1128[246]; // 0x1128 - 0x14FC
    uint32_t hier;      // 0x1500
} pruIntc;

/* PRU CFG register set */
typedef struct {
    uint32_t revid;     // 0x0
    uint32_t syscfg;    // 0x4
    uint32_t gpcfg0;    // 0x8
    uint32_t gpcfg1;    // 0xC
    uint32_t cgr;       // 0x10
    uint32_t isrp;      // 0x14
    uint32_t isp;       // 0x18
    uint32_t iesp;      // 0x1C
    uint32_t iecp;      // 0x20
    uint32_t rsvd24;    // 0x24
    uint32_t pmao;      // 0x28
    uint32_t mii_rt;    // 0x2C
    uint32_t iepclk;    // 0x30
    uint32_t spp;       // 0x34
    uint32_t rsvd38;    // 0x38
    uint32_t rsvd40;    // 0x40
    uint32_t pin_mx;    // 0x44
} pruCfg;

/* PRU IEP register set */
typedef struct {
    uint32_t global_cfg;    // 0x0
    uint32_t global_status; // 0x4
    uint32_t compen;    // 0x8
    uint32_t count;     // 0xC
    uint32_t rsvd10[12];    // 0x10 - 0x3C
    uint32_t cmp_cfg;   // 0x40
    uint32_t cmp_status;    // 0x44
    uint32_t cmp0;      // 0x48
    uint32_t cmp1;      // 0x4C
    uint32_t cmp2;      // 0x50
    uint32_t cmp3;      // 0x54
    uint32_t cmp4;      // 0x58
    uint32_t cmp5;      // 0x5C
    uint32_t cmp6;      // 0x60
    uint32_t cmp7;      // 0x64
} pruIep;

/* Map the two user-accessible CPU registers to variables */
volatile register uint32_t __R30;
volatile register uint32_t __R31;

/*
 * Map constant table register to variable. Use 'far' for the next variable
 * since the data for the interrupt controller can't be completely contained
 * within the first 256 bytes from the top of the constant register pointer.
 * Note that 'far' has to be used for both the variable declaration as well as
 * inside the cregister statement.
 */
volatile far pruIntc CT_INTC __attribute__((cregister("C0_INTC", far), peripheral));
volatile pruCfg CT_CFG __attribute__((cregister("C4_CFG", near), peripheral));
volatile pruIep CT_IEP __attribute__((cregister("C26_IEP", near), peripheral));

#endif /* PRU_H_ */
