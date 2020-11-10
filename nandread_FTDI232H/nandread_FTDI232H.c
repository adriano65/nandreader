/* 
 * This file is part of the ftdi-nand-flash-reader distribution (https://github.com/maehw/ftdi-nand-flash-reader).
 * Copyright (c) 2018 maehw.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * \file bitbang_ft2232.c
 * \brief NAND flash reader based on FTDI FT2232 IC in bit-bang IO mode
 * Interfacing NAND flash devices with an x8 I/O interface for address and data.
 * Additionally the signals Chip Enable (nCE), Write Enable (nWE), Read Enable (nRE), 
 * Address Latch Enable (ALE), Command Latch Enable (CLE), Write Protect (nWP) 
 * and Ready/Busy (RDY) on the control bus are used.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h> 
#include <libftdi1/ftdi.h>

int brcm_nand(unsigned poly, unsigned char *data);

#define NANDREADER_DBG_MIN
#ifdef NANDREADER_DBG_MIN
  #define DBG_MIN(fmt...) do {printf("%s: ", __FUNCTION__); printf(fmt); printf("\n"); } while(0)
#else
  #define DBG_MIN(fmt...) do { } while(0)
#endif
//#define NANDREADER_DBG_MAX
#ifdef NANDREADER_DBG_MAX
  #define DBG_MAX(fmt...) do {printf("%s: ", __FUNCTION__); printf(fmt); printf("\n"); } while(0)
#else
  #define DBG_MAX(fmt...) do { } while(0)
#endif

/* FTDI FT2232H VID and PID */
#define FT2232H_VID 0x0403
//#define FT2232H_PID 0x6010
#define FT2232H_PID 0x6014

/*
FTDI PIN                  NAND PIN
21        ACBUS0  CS#     9         CE#
25        ACBUS1  A0      19        WP#
26        ACBUS2  RD#     8         RE#
27        ACBUS3  WR#     18        WE#
28        ACBUS4  SIWU#   19        WP# test --> NOK 
29        ACBUS5  ACBUS5  17        ALE
30        ACBUS6  ACBUS6  16        CLE
31        ACBUS7  PWRSAV# 19        WP# test --> NOK 
32        ACBUS8  ACBUS8
33        ACBUS9  ACBUS9
*/

/* Pins on ADBUS0..7 (I/O bus) */
#define PIN_DIO0 0x01
#define PIN_DIO1 0x02
#define PIN_DIO2 0x04
#define PIN_DIO3 0x08
#define PIN_DIO4 0x10
#define PIN_DIO5 0x20
#define PIN_DIO6 0x40
#define PIN_DIO7 0x80
#define IOBUS_BITMASK_WRITE 0xFF
#define IOBUS_BITMASK_READ  0x00

/* Pins on ACBUS0..7 (control bus) */
#define PIN_nCE  0x01   //ACBUS0
#define PIN_nWP  0x02   //ACBUS1
#define PIN_nRE  0x04   //ACBUS2
#define PIN_nWE  0x08   //ACBUS3 - nand pin 18  
#define PIN_RDY  0x10   //ACBUS4 - nand pin 7  
#define PIN_ALE  0x20
#define PIN_CLE  0x40
//#define PIN_nWP  0x80
#define CONTROLBUS_DIR 0b01101111   // pinDirection 0=in, 1=out

#define STATUSREG_IO0  0x01

#define REALWORLD_DELAY 10 /* 10 usec */
//#define REALWORLD_DELAY 300
//#define REALWORLD_DELAY 1

typedef enum { 
  MT29,
  S34,
  H27,
  BAD_CONTACT,
  } nand_type;

#define MAX_NAND 16
typedef struct _sNand {
  unsigned char name[80];
  unsigned char ID_register_exp[5];
  unsigned int page_size;
  unsigned int spare_size;
  unsigned int block_size;
  unsigned char type;
} sNand;
unsigned char nIdx;


sNand Nand[MAX_NAND] = 
{ 
  // Micron MT29F1G08ABADAWP 128 MB - Erase Block size 128 kB, spare area 64 B, page size 2kB
  {"MT29F1G08ABADAWP", {0x2C, 0xF1, 0x80, 0x95, 0x02}, 2048, 64, 64, MT29},
  {"MT29F1G08ABADAWP (ECC enabled)", {0x2C, 0xF1, 0x80, 0x95, 0x82}, 2048, 64, 64, MT29},
  {"MT29F1G08ABADAWP-reset?", {0x2C, 0x01, 0x02, 0x03, 0x04}, 2048, 64, 64, MT29},
  {"S34ML01G100TF100", {0x01, 0xF1, 0x00, 0x1D, 0x01}, 2048, 64, 64, S34},  // Cypress
  // Hynix H27UCG8T2ATR-BC 8 MB - Erase Block Size : 256 kB + 160K bytes, 256pages - Page Size : 8kB + 640(Spare) 
  {"H27UCG8T2ATR-BC", {0xAD, 0xDE, 0x94, 0xDA, 0x74}, 8192, 640, 256, H27},
  {"Bad contact---", {0x00, 0xD7, 0x00, 0x10, 0x10}, 2048, 64, 64, 4},
  {"Tappo veloce", {0x00, 0xD7, 0x00, 0x10, 0x10}, 2048, 64, 64, 0}
};

unsigned char CMD_RESET[1] = { 0xFF }; /* read ID register */
unsigned char CMD_READID[2] = { 0x90, 0x00 }; /* read ID register */
unsigned char CMD_READ1[1] = { 0x00 }; /* page read start*/
unsigned char CMD_READ2[1] = { 0x30 }; /* page read end */
unsigned char CMD_EN_ECC[1] = { 0xEF };
unsigned char CMD_BLOCKERASE1[1] = { 0x60 }; /* block erase */
unsigned char CMD_BLOCKERASE2[1] = { 0xD0 }; /* block erase */
unsigned char CMD_READSTATUS[1] = { 0x70 }; /* read status */
unsigned char CMD_READSTATUS2[1] = { 0x00 }; /* end read status when reading pages with ECC*/
unsigned char CMD_PAGEPROGRAM1[1] = { 0x80 }; /* program page */
unsigned char CMD_PAGEPROGRAM2[1] = { 0x10 }; /* program page */

typedef enum { LOW=0, HIGH=1 } onoff_t;
typedef enum { IOBUS_IN=0, IOBUS_OUT=1 } iobus_inout_t;

unsigned char iobus_value;
unsigned char controlbus_dir=CONTROLBUS_DIR, controlbus_value;
unsigned char crtl_buf[4], io_buf[4];
unsigned char column_addr[2], row_addr[3];

struct ftdi_context *pBusInterface;

int msleep(long msec) {
    struct timespec ts;
    int res;

    if (msec < 0) { errno = EINVAL; return -1; }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

void controlbus_pin_set(unsigned char pin, onoff_t val) {
    if(val == HIGH)
        controlbus_value |= pin;
    else
        controlbus_value &= (unsigned char)0xFF ^ pin;
}

void controlbus_update_output() {
    crtl_buf[0] = SET_BITS_HIGH;
    crtl_buf[1] = controlbus_value;       // pinState
    crtl_buf[2] = controlbus_dir;         // pinDirection 0=in, 1=out
    if ( ftdi_write_data(pBusInterface, crtl_buf, 3) < 0 ) printf("ftdi_write_data fail\n");
}

// nand data bus - ADBUS
void iobus_set_direction(iobus_inout_t inout) {
    io_buf[0] = SET_BITS_LOW;
    io_buf[1] = 0xFF;       // pinState
    io_buf[2] = (inout == IOBUS_OUT) ? 0xFF : 0x00;       // pinDirection 0=in, 1=out
    if ( ftdi_write_data(pBusInterface, io_buf, 3) < 0 ) printf("ftdi_write_data fail\n");
}

// nand data bus - ADBUS
void iobus_update_output() {
    io_buf[0] = SET_BITS_LOW;
    io_buf[1] = iobus_value;  // pinState
    io_buf[2] = 0xFF;         // pinDirection 0=in, 1=out
    if ( ftdi_write_data(pBusInterface, io_buf, 3) < 0 ) printf("ftdi_write_data fail\n");
}

unsigned char controlbus_read_input() {
    unsigned char buf;
    ftdi_read_pins(pBusInterface, &buf);
    return buf;
}


/* Command Input bus operation is used to give a command to the memory device. Command are accepted with Chip
Enable low, Command Latch Enable High, Address Latch Enable low and Read Enable High and latched on the rising
edge of Write Enable. Moreover for commands that starts a modify operation (write/erase) the Write Protect pin must be
high. */
int latch_command(unsigned char *command, unsigned char count) {
    /* check if ALE is low and nRE is high */
    if( controlbus_value & PIN_nCE ) { fprintf(stderr, "latch_command requires nCE pin to be low\n"); return EXIT_FAILURE; }
    else if( ~controlbus_value & PIN_nRE ) { fprintf(stderr, "latch_command requires nRE pin to be high\n"); return EXIT_FAILURE; }

    /* debug info */
    DBG_MAX("latch_command(0x%02X)\n", command[0]);

    /* set CLE high (activates the latching of the IO inputs inside the Command Register on the Rising edge of nWE) */
    DBG_MAX("  setting CLE high\n");
    controlbus_pin_set(PIN_CLE, HIGH);
    controlbus_update_output();
    controlbus_pin_set(PIN_nWE, LOW);
    controlbus_update_output();
    DBG_MAX("  setting I/O bus to command\n");
    iobus_value=command[0];
    iobus_update_output();

    // toggle nWE back high (acts as clock to latch the command!)
    controlbus_pin_set(PIN_nWE, HIGH);
    controlbus_update_output();

    controlbus_pin_set(PIN_CLE, LOW);
    controlbus_update_output();

    if (count>1) {
      // address  -------------------------- --------------
      controlbus_pin_set(PIN_ALE, HIGH);
      controlbus_update_output();

      controlbus_pin_set(PIN_nWE, LOW);
      controlbus_update_output();
      usleep(REALWORLD_DELAY);

      // change I/O pins
      iobus_value=command[1];
      iobus_update_output();
      usleep(REALWORLD_DELAY); /* TODO: assure setup delay */

      // toggle nWE back high (acts as clock to latch the current address byte!)
      controlbus_pin_set(PIN_nWE, HIGH);
      controlbus_update_output();
      usleep(REALWORLD_DELAY); /* TODO: assure hold delay */

      // toggle ALE low
      controlbus_pin_set(PIN_ALE, LOW);
      controlbus_update_output();
      }
    return 0;
}

/** 
 * "Address Input bus operation allows the insertion of the memory address. 
 * Five cycles are required to input the addresses for the 4Gbit devices. 
 * Addresses are accepted with Chip Enable low, Address Latch Enable High, Command Latch Enable low and 
 * Read Enable High and latched on the rising edge of Write Enable.
 * Moreover for commands that starts a modifying operation (write/erase) the Write Protect pin must be high. 
 * See Figure 5 and Table 13 for details of the timings requirements.
 * Addresses are always applied on IO7:0 regardless of the bus configuration (x8 or x16).""
 */
int setcolumns(uint32_t mem_address) {
    if( controlbus_value & PIN_nCE ) { fprintf(stderr, "nCE pin is not low\n"); return EXIT_FAILURE; }
    else if( controlbus_value & PIN_CLE ) { fprintf(stderr, "CLE pin is not low\n"); return EXIT_FAILURE; }
    else if( ~controlbus_value & PIN_nRE ) { fprintf(stderr, "nRE pin is not high\n"); return EXIT_FAILURE; }

    /* toggle ALE high (activates the latching of the IO inputs inside the Address Register on the Rising edge of nWE. */
    controlbus_pin_set(PIN_ALE, HIGH);
    controlbus_update_output();

    switch (Nand[nIdx].type) {
      case MT29:
        column_addr[0] = (unsigned char)(  mem_address & 0x000000FF);
        column_addr[1] = (unsigned char)( (mem_address & 0x00000F00) >> 8 );
        break;

      case S34:
        column_addr[0] = (unsigned char)(  mem_address & 0x000000FF);
        column_addr[1] = (unsigned char)( (mem_address & 0x00000F00) >> 8 );
        break;

      case H27:
        column_addr[0] = (unsigned char)(  mem_address & 0x000000FF);
        column_addr[1] = (unsigned char)( (mem_address & 0x00003F00) >> 8 );
        break;

      default:
        printf("Not supported"); exit(1);
        }
    for(int i = 0; i < 2; i++) {
      controlbus_pin_set(PIN_nWE, LOW);
      controlbus_update_output();
      usleep(REALWORLD_DELAY);

      // change I/O pins
      iobus_value=column_addr[i];
      iobus_update_output();
      usleep(REALWORLD_DELAY); /* TODO: assure setup delay */

      // toggle nWE back high (acts as clock to latch the current address byte!)
      controlbus_pin_set(PIN_nWE, HIGH);
      controlbus_update_output();
      usleep(REALWORLD_DELAY); /* TODO: assure hold delay */
      }

    //controlbus_pin_set(PIN_ALE, LOW);
    //controlbus_update_output();

    // wait for ALE to nRE Delay tAR before nRE is taken low (nanoseconds!)
    //usleep(REALWORLD_DELAY);
    return 0;
}

int setrows(uint32_t mem_address) {
    unsigned char rows;
    unsigned int nBlock;
    /* check if ALE is low and nRE is high */
    if( controlbus_value & PIN_nCE ) { fprintf(stderr, "nCE pin is not low\n"); return EXIT_FAILURE; }
    else if( controlbus_value & PIN_CLE ) { fprintf(stderr, "CLE pin is not low\n"); return EXIT_FAILURE; }
    else if( ~controlbus_value & PIN_nRE ) { fprintf(stderr, "nRE pin is not high\n"); return EXIT_FAILURE; }

    /* toggle ALE high (activates the latching of the IO inputs inside the Address Register on the Rising edge of nWE. */
    controlbus_pin_set(PIN_ALE, HIGH);
    controlbus_update_output();

    switch (Nand[nIdx].type) {
      case MT29:
        nBlock=mem_address%Nand[nIdx].page_size;
        //row_addr[0] = (unsigned char)( (mem_address & 0x000FF000) >> 12 );
        //row_addr[1] = (unsigned char)( (mem_address & 0x0FF00000) >> 20 );
        //row_addr[0] = (unsigned char)( (mem_address & 0x0000F700) >> 12 );
        //row_addr[1] = (unsigned char)( (mem_address & 0x00FF0000) >> 20 );
        row_addr[0] = (unsigned char)(mem_address >> 12 );
        row_addr[1] = (unsigned char)(mem_address >> 20 );
        //row_addr[0] = (unsigned char)(mem_address >> 16 );
        //row_addr[1] = (unsigned char)(mem_address >> 24 );
        rows=2;
        break;
      case S34:
        row_addr[0] = (unsigned char)( (mem_address & 0x000FF000) >> 12 );
        row_addr[1] = (unsigned char)( (mem_address & 0x0FF00000) >> 20 );
        row_addr[2] = (unsigned char)( (mem_address & 0x30000000) >> 28 );
        rows=3;
        break;
      case H27:
        row_addr[0] = (unsigned char)( (mem_address & 0x000FC000) >> 12 );
        row_addr[1] = (unsigned char)( (mem_address & 0x0FF00000) >> 20 );
        row_addr[2] = (unsigned char)( (mem_address & 0xF0000000) >> 28 );
        rows=3;
        break;

      default:
        printf("Not supported"); exit(1);
        }

    for(int i = 0; i < rows; i++) {
        // toggle nWE low
        controlbus_pin_set(PIN_nWE, LOW);
        controlbus_update_output();
        usleep(REALWORLD_DELAY);

        // change I/O pins
        iobus_value=row_addr[i];
        iobus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure setup delay */

        // toggle nWE back high (acts as clock to latch the current address byte!)
        controlbus_pin_set(PIN_nWE, HIGH);
        controlbus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure hold delay */
    }

    // toggle ALE low
    controlbus_pin_set(PIN_ALE, LOW);
    controlbus_update_output();

    // wait for ALE to nRE Delay tAR before nRE is taken low (nanoseconds!)
    //usleep(REALWORLD_DELAY);
    return 0;
}


/* Data Output bus operation allows to read data from the memory array and to 
 * check the status register content, the EDC register content and the ID data.
 * Data can be serially shifted out by toggling the Read Enable pin with Chip 
 * Enable low, Write Enable High, Address Latch Enable low, and Command Latch 
 * Enable low. */
int read_data(unsigned char InBuf[], unsigned int length) {
    unsigned int addr_idx = 0;
    int f;

    /* check if ALE is low and nRE is high */
    if( controlbus_value & PIN_nCE ) { fprintf(stderr, " requires nCE pin to be low\n"); return EXIT_FAILURE; }
    else if( ~controlbus_value & PIN_nWE ) { fprintf(stderr, " requires nWE pin to be high\n"); return EXIT_FAILURE; }
    else if( controlbus_value & PIN_ALE ) { fprintf(stderr, " requires ALE pin to be low\n"); return EXIT_FAILURE; }

    iobus_set_direction(IOBUS_IN);

    for(addr_idx = 0; addr_idx < length; addr_idx++) {
        /* toggle nRE low; acts like a clock to latch out the data; data is valid tREA after the falling edge of nRE 
         * (also increments the internal column address counter by one) */
        controlbus_pin_set(PIN_nRE, LOW);
        controlbus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure tREA delay */

        // read I/O pins
        if ((f=ftdi_read_pins(pBusInterface, &InBuf[addr_idx])) < 0 ) { DBG_MIN("Err"); }

        // toggle nRE back high
        controlbus_pin_set(PIN_nRE, HIGH);
        controlbus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure tREH and tRHZ delays */
    }

    iobus_set_direction(IOBUS_OUT);
    return 0;
}

int check_ID_register(unsigned char* ID_register) {
    int nRet=0;
    for (nIdx=0; nIdx<MAX_NAND; nIdx++) {
      if( strncmp( (char *)Nand[nIdx].ID_register_exp, (char *)ID_register, 5 ) == 0 ) { 
        DBG_MIN("ID 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", Nand[nIdx].ID_register_exp[0], Nand[nIdx].ID_register_exp[1], Nand[nIdx].ID_register_exp[2], Nand[nIdx].ID_register_exp[3], Nand[nIdx].ID_register_exp[4]); 
        DBG_MIN("%s found, page size %u, block size %u", Nand[nIdx].name, Nand[nIdx].page_size, Nand[nIdx].block_size); 
        
        nRet=1;
        break;
        }
      }
    if (nIdx == MAX_NAND) { 
      DBG_MIN("FAIL: ID register did not match"); 
      DBG_MIN("ID register read:   0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", ID_register[0], ID_register[1], ID_register[2], ID_register[3], ID_register[4] ); 
      }
    return nRet;
}

int read_memory(unsigned int nPage, unsigned char *data, unsigned int page_size) {
    uint32_t mem_address;

    // Start reading the data
    mem_address = nPage * page_size; // add bytes per block (i.e. goto next block)
    // 1073741824 == 1 Gbit, 1073741824/8 == 134217728 bytes == 128 MB
    // Block size 128 kB, spare area 64 B, page size 2kB
    // 128 kB / 2 kb == 64, 128 Mb / 128 kB == 1024 blocks
    // total block are 64 * 1024;
    DBG_MIN("Reading data from 0x%02X from page %d", mem_address, nPage);
    latch_command(CMD_READ1, 1);
    setcolumns(mem_address);
    setrows(mem_address);
    latch_command(CMD_READ2, 1);

    DBG_MAX("Checking for busy line...");
    while( !(controlbus_read_input() & PIN_RDY) );

    /*
    DBG_MIN("Latching command byte to read status...");
    latch_command(CMD_READSTATUS, 1);
    unsigned char status_register;
    read_data(&status_register, 1);
    latch_command(CMD_READSTATUS2, 1);
    DBG_MIN("Status register content: 0x%02x", status_register);
    if(status_register & STATUSREG_IO0)	{ DBG_MIN("Failed to read page.");	return; }
    else { DBG_MIN("Successfully read nBlock %d", nBlock ); }
    */


    DBG_MAX("Latching out data block...");
    read_data(data, page_size);
    return 0;
}

/**
 * BlockErase
 *
 * "The Erase operation is done on a block basis.
 * Block address loading is accomplished in there cycles initiated by an Erase Setup command (60h).
 * Only address A18 to A29 is valid while A12 to A17 is ignored (x8).
 *
 * The Erase Confirm command (D0h) following the block address loading initiates the internal erasing process.
 * This two step sequence of setup followed by execution command ensures that memory contents are not
 * accidentally erased due to external noise conditions.
 *
 * At the rising edge of WE after the erase confirm command input,
 * the internal write controller handles erase and erase verify.
 *
 * Once the erase process starts, the Read Status Register command may be entered to read the status register.
 * The system controller can detect the completion of an erase by monitoring the R/B output,
 * or the Status bit (I/O 6) of the Status Register.
 * Only the Read Status command and Reset command are valid while erasing is in progress.
 * When the erase operation is completed, the Write Status Bit (I/O 0) may be checked."
 */
int erase_page(unsigned int starting_page, unsigned int totpages) {
  // min erase size = 0x20000 == 128 kB
  // one erase_page == 64 * 2048 blocks
	uint32_t mem_address;

  for( unsigned int nPage = starting_page; nPage < (starting_page+totpages); nPage++ ) {
    /* calculate memory address */
    mem_address = Nand[nIdx].page_size * Nand[nIdx].block_size * nPage;

    /* remove write protection */
    controlbus_pin_set(PIN_nWP, HIGH);

    DBG_MAX("Latching first command byte to erase a page...");
    latch_command(CMD_BLOCKERASE1, 1); /* block erase setup command */

    DBG_MAX("Erasing page from memory address 0x%02X", mem_address);
    DBG_MAX("Latching page(row) address (3 bytes)...\n");
    setrows(mem_address);

    DBG_MAX("Latching second command byte to erase a page...");
    latch_command(CMD_BLOCKERASE2, 1);

    /* tWB: WE High to Busy is 100 ns -> ignore it here as it takes some time for the next command to execute */

    // busy-wait for high level at the busy line
    DBG_MAX("Checking for busy line...");
    while( !(controlbus_read_input() & PIN_RDY) );

    /* Read status */
    DBG_MAX("Latching command byte to read status...\n");
    latch_command(CMD_READSTATUS, 1);

    unsigned char status_register;
    read_data(&status_register, 1); /* data output operation */

    /* output the retrieved status register content */
    printf("Status register content: 0x%02X\n", status_register);

    /* activate write protection again */
    controlbus_pin_set(PIN_nWP, LOW);

    if(status_register & STATUSREG_IO0)	{ fprintf(stderr, "Failed to erase page.\n"); }
    else { printf("Successfully erased page.\n"); }
    }
  return 0;
}


int latch_data_out(unsigned char data[], unsigned int length) {
    for(unsigned int k = 0; k < length; k++) {
        // toggle nWE low
        controlbus_pin_set(PIN_nWE, LOW);
        controlbus_update_output();
        usleep(REALWORLD_DELAY);

        // change I/O pins
        iobus_value=data[k];        
        iobus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure setup delay */

        DBG_MAX("0x%04X 0x%02X ", k, data[k]);

        // toggle nWE back high (acts as clock to latch the current address byte!)
        controlbus_pin_set(PIN_nWE, HIGH);
        controlbus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure hold delay */
    }

    return 0;
}

/**
 * Page Program
 *
 * "The device is programmed by page.
 * The number of consecutive partial page programming operation within the same page
 * without an intervening erase operation must not exceed 8 times.
 *
 * The addressing should be done on each pages in a block.
 * A page program cycle consists of a serial data loading period in which up to 2112 bytes of data
 * may be loaded into the data register, followed by a non-volatile programming period where the loaded data
 * is programmed into the appropriate cell.
 *
 * The serial data loading period begins by inputting the Serial Data Input command (80h),
 * followed by the five cycle address inputs and then serial data.
 *
 * The bytes other than those to be programmed do not need to be loaded.
 *
 * The device supports random data input in a page.
 * The column address of next data, which will be entered, may be changed to the address which follows
 * random data input command (85h).
 * Random data input may be operated multiple times regardless of how many times it is done in a page.
 *
 * The Page Program confirm command (10h) initiates the programming process.
 * Writing 10h alone without previously entering the serial data will not initiate the programming process.
 * The internal write state controller automatically executes the algorithms and timings necessary for
 * program and verify, thereby freeing the system controller for other tasks.
 * Once the program process starts, the Read Status Register command may be entered to read the status register.
 * The system controller can detect the completion of a program cycle by monitoring the R/B output,
 * or the Status bit (I/O 6) of the Status Register.
 * Only the Read Status command and Reset command are valid while programming is in progress.
 *
 * When the Page Program is complete, the Write Status Bit (I/O 0) may be checked.
 * The internal write verify detects only errors for "1"s that are not successfully programmed to "0"s.
 *
 * The command register remains in Read Status command mode until another valid command is written to the
 * command register.
 */
int program_page(unsigned int nPage, unsigned char* data, unsigned int page_size) {
  int n;
	uint32_t mem_address;

  mem_address = nPage * page_size;

  for (n=0; n<page_size; n++) { if (data[n]!=0xFF) break; }
  if (n==page_size) { DBG_MIN("skip page %d", nPage); return 0; }
  /* remove write protection */
  controlbus_pin_set(PIN_nWP, HIGH);

  if( ~controlbus_value & PIN_nRE ) { fprintf(stderr, "nRE pin is not high\n"); return EXIT_FAILURE; }

  DBG_MIN("Writing from 0x%04X to 0x%04X, Page %d", mem_address, mem_address+page_size-1, nPage);
  //for (int n=0; n<page_size; n+=8) { DBG_MIN("mem_address 0x%04X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", mem_address+n, data[n], data[n+1], data[n+2], data[n+3], data[n+4], data[n+5], data[n+6], data[n+7]); }
  latch_command(CMD_PAGEPROGRAM1, 1); /* Serial Data Input command */

  DBG_MAX("Latching address cycles...");
  setcolumns(mem_address);
  setrows(mem_address);

  DBG_MAX("Latching out the data of the page...");
  latch_data_out(data, page_size);

  DBG_MAX("Latching second command byte to write a page...");
  latch_command(CMD_PAGEPROGRAM2, 1); /* Page Program confirm command */

  // busy-wait for high level at the busy line
  DBG_MAX("Checking for busy line...");
  while( !(controlbus_read_input() & PIN_RDY) );

  /* Read status */
  DBG_MAX("Latching command byte to read status...");
  latch_command(CMD_READSTATUS, 1);

  unsigned char status_register;
  read_data(&status_register, 1); /* data output operation */

  /* output the retrieved status register content */
  DBG_MAX("Status register content: 0x%02n", status_register);

  /* activate write protection again */
  controlbus_pin_set(PIN_nWP, LOW);

  if(status_register & 0x04)	{ fprintf(stderr, " Rewrite recommended.");	return 0; }
  
  if(status_register & STATUSREG_IO0)	{ fprintf(stderr, "Failed to program page.");	return 0; }
  else { DBG_MAX("Successfully programmed nPageId %d", nPageId ); }

  return 0;
}

// EFh(cmd)-90h(addr)-08h(data)-00h(data)-00h(data)-00h(data) --> enable
int set_ecc(unsigned char enable) {
  unsigned char data[4] = { 0x00, 0x00, 0x00, 0x00 };
  if (enable) data[0]=0x08;

  latch_command(CMD_EN_ECC, 1);

  // address
  controlbus_pin_set(PIN_ALE, HIGH);
  controlbus_update_output();
  controlbus_pin_set(PIN_nWE, LOW);
  controlbus_update_output();
  usleep(REALWORLD_DELAY);
  iobus_value=0x90;       // Address 
  iobus_update_output();
  usleep(REALWORLD_DELAY); /* TODO: assure setup delay */
  controlbus_pin_set(PIN_nWE, HIGH);  // toggle nWE back high (acts as clock to latch the current address byte!)
  controlbus_update_output();
  usleep(REALWORLD_DELAY);
  controlbus_pin_set(PIN_ALE, LOW);
  controlbus_update_output();


  latch_data_out(data, 4);

  return 1;
}



void HandleSig(){
  return;
  //if (fp!=NULL) fclose(fp);
}

// MT29F4G08ABADA -- NOT F1 like it seems!
int main(int argc, char **argv) {
  struct ftdi_version_info version;
  FILE *fp, *fpOut;
  unsigned int FTDIchipID, totPages, toterasepages, enable;
  unsigned int startingpage=0xFFFF, starting_erasepage=0xFFFF;
  unsigned char ID_register[5];
  int f, opt;
  char filename[256]="?", filenameOut[256]="?";
  char *endptr;
  extern char *optarg;

  //signal(SIGINT, HandleSig);

  if ((pBusInterface = ftdi_new()) == 0) {
      DBG_MIN("ftdi_new failed for iobus");
      return EXIT_FAILURE;
    }
  ftdi_set_interface(pBusInterface, INTERFACE_A);
  if ( (f=ftdi_usb_open(pBusInterface, FT2232H_VID, FT2232H_PID)) != 0 ) {
      fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f, ftdi_get_error_string(pBusInterface));
      ftdi_free(pBusInterface);
      exit(-1);
    }
  pBusInterface->bitbang_enabled=0;

  ftdi_read_chipid(pBusInterface, &FTDIchipID);
  printf("ftdi chipid = 0x%04X\n", FTDIchipID);

  ftdi_usb_reset(pBusInterface);
  ftdi_set_interface(pBusInterface, INTERFACE_ANY);
  ftdi_set_bitmode(pBusInterface, 0, 0); // reset
  ftdi_set_bitmode(pBusInterface, 0, BITMODE_MPSSE);
  //ftdi_set_bitmode(pBusInterface, 0, BITMODE_MCU);
  ftdi_usb_purge_buffers(pBusInterface);
  msleep(50); // sleep 50 ms for setup to complete

  /*
  buf[icmd++] = WRITE_EXTENDED;
  buf[icmd++] = 0x00;
  buf[icmd++] = 0xFF;
  buf[icmd++] = WRITE_SHORT;
  buf[icmd++] = 0x00;
  */
  
  iobus_set_direction(IOBUS_IN);
  // set nRE high and nCE and nWP low
  controlbus_pin_set(PIN_nRE, HIGH);
  controlbus_pin_set(PIN_nCE, LOW);
  controlbus_pin_set(PIN_nWP, LOW); /* When WP# is LOW, PROGRAM and ERASE operations are disabled */
  controlbus_update_output();

  latch_command(CMD_RESET, 1); /* command input operation; command: RESET */
  msleep(10);
  set_ecc(0);
  // Read the ID register
  DBG_MIN("Trying to read the ID register...");
  latch_command(CMD_READID, 2); /* command input operation; command: READ ID */
  read_data(ID_register, 5); /* data output operation */

  if ( check_ID_register(ID_register) ) {
    while ((opt = getopt(argc, argv, "c:p:e:E:s:r:w:W:R:f:v")) != -1) {		//: semicolon means that option need an arg!
      switch(opt) {
        case 'E' :
          enable = (unsigned int)strtoul(optarg, &endptr, 0);
          if (enable) set_ecc(1);
          else set_ecc(0);
          break;

        case 'e' :
          if (starting_erasepage==0xFFFF) {
            DBG_MIN("Set starting page (-p option)");
            }
          else {
            toterasepages = (unsigned int)strtoul(optarg, &endptr, 0);
            // bootloader ->  128 * 2k = 256 k
            erase_page(starting_erasepage, toterasepages);
            }
          break;

        case 'v' :
          printf("NandReader ver 0.0.1\n");
          version = ftdi_get_library_version();
          printf("libftdi %s (major: %d, minor: %d, micro: %d, snapshot ver: %s)\n", version.version_str, version.major, version.minor, version.micro, version.snapshot_str);
          break;

        case 'f' :
          sprintf(filename, "%s", optarg);
          //printf("%s\n",filename);
          break;

        case 'p' :
          starting_erasepage = (unsigned int)strtoul(optarg, &endptr, 0);
          break;

        case 's' :
          startingpage = (unsigned int)strtoul(optarg, &endptr, 0);
          break;

        // DO NOT MOVE before -f !!!!!
        case 'c' :
          sprintf(filenameOut, "%s", optarg);
          if (strlen(filename)>2) {
            if ( (fp = fopen(filename, "rb")) == NULL ) { DBG_MIN("Error when opening the file %s", filename); break; }
            unsigned char data[Nand[nIdx].page_size+Nand[nIdx].spare_size];
            if ( (fpOut = fopen(filenameOut, "wb+")) == NULL ) { DBG_MIN("Error when opening the file %s", filenameOut); break; }
            while (!feof(fp)) {
              fread(&data[0], 1, Nand[nIdx].page_size+Nand[nIdx].spare_size, fp);
              fwrite(&data[0], 1, Nand[nIdx].page_size, fpOut);
              }
            fclose(fpOut);
            fclose(fp);
            }
          else { DBG_MIN("filename missed (-f option)");}
          break;

        case 'r' :
          if (startingpage==0xFFFF) {
            DBG_MIN("Set starting block (-s option)");
            }
          else {
            if (strlen(filename)>2) {
              totPages = (unsigned int)strtoul(optarg, &endptr, 0);
              if ( (fp = fopen(filename, "wb+")) == NULL ) { DBG_MIN("Error when opening the file %s", filename); break; }
              unsigned char *data=(unsigned char *)malloc(Nand[nIdx].page_size);
              // bootloader ->  128 * 2k = 256 k 
              // total blocks -> 64 * 1024
              for (unsigned int nPage = startingpage; nPage < startingpage+totPages; nPage++) {
                read_memory(nPage, data, Nand[nIdx].page_size);
                //for (int i=0; i<Nand[nIdx].page_size; i++) printf("Page 0x%02X Data 0x%02X\n", i, data[i]);
                fwrite(data, 1, Nand[nIdx].page_size, fp);
                }
              fclose(fp);
              free(data);
              }
            else { DBG_MIN("filename missed (-f option)");}
            }
          break;

        case 'R' :
          if (startingpage==0xFFFF) {
            DBG_MIN("Set starting block (-s option)");
            }
          else {
            if (strlen(filename)>2) {
              if ( (fp = fopen(filename, "wb+")) == NULL ) { DBG_MIN("  Error when opening the file..."); break; }
              totPages = (unsigned int)strtoul(optarg, &endptr, 0);
              unsigned char *data=(unsigned char *)malloc(Nand[nIdx].page_size);
              for (unsigned int nPage = startingpage; nPage < startingpage+totPages; nPage++) {
                read_memory(nPage, data, Nand[nIdx].page_size+Nand[nIdx].spare_size);
                //for (int i=0; i<Nand[nIdx].page_size; i++) printf("Page 0x%02X Data 0x%02X\n", i, data[i]);
                fwrite(&data[0], 1, Nand[nIdx].page_size+Nand[nIdx].spare_size, fp);
                }
              fclose(fp);
              free(data);
              }
            else { DBG_MIN("filename missed (-f option)");}
            }
          break;

        case 'w' :
          if (startingpage==0xFFFF) {
            DBG_MIN("Set starting block (-s option)");
            }
          else {
            if (strlen(filename)>2) {
              totPages = (unsigned int)strtoul(optarg, &endptr, 0);
              // this is enought for first erased page of 128 kB
              //unsigned char data[2048*64];
              // but we have a 256 kB file (0x40000)
              //unsigned char data[2048*128];
              // nooo we have a 512 kB file...

              if ( (fp = fopen(filename, "rb")) == NULL ) { printf("Error when opening the file...\n"); break; }
              //fseek(fp, Nand[nIdx].page_size*startingpage, SEEK_SET);
              unsigned char *data=(unsigned char *)malloc(Nand[nIdx].page_size);
              for (unsigned int nPage = startingpage; nPage < startingpage+totPages; nPage++) {
                if (feof(fp)) { printf("End of file %s\n", filename); break; }
                fread(data, 1, Nand[nIdx].page_size, fp);
                //for (int i=0; i<Nand[nIdx].page_size; i++) printf("Page 0x%02X Data 0x%02X\n", i, data[i]);
                program_page(nPage, data, Nand[nIdx].page_size);
                }
              fclose(fp);
              free(data);
              }
            else { DBG_MIN("filename missed (-f option)");}
            }

          break;

        case 'W' :
          if (startingpage==0xFFFF) {
            DBG_MIN("Set starting block (-s option)");
            }
          else {
            if (strlen(filename)>2) {
              if ( (fp = fopen(filename, "rb")) == NULL ) { printf("Error when opening the file...\n"); break; }
              //fseek(fp, (Nand[nIdx].page_size+Nand[nIdx].spare_size)*startingpage, SEEK_SET);
              unsigned char *data=(unsigned char *)malloc(Nand[nIdx].page_size+Nand[nIdx].spare_size);
              for (unsigned int nPage = startingpage; nPage < startingpage+totPages; nPage++) {
                if (feof(fp)) { printf("End of file %s\n", filename); break; }
                fread(&data[0], 1, Nand[nIdx].page_size+Nand[nIdx].spare_size, fp);
                brcm_nand(0, data);   //default polynomial
                program_page(nPage, data, Nand[nIdx].page_size+Nand[nIdx].spare_size);
                }
              fclose(fp);
              free(data);
              }
            else { DBG_MIN("filename missed (-f option)");}
            }
          break;

        default:
          printf("-s startingpage -r 256 -> reads from startingpage total pages (first 256 blocks is bootloader, second 256 block certificates)\n");
          printf("-p 0 -e 2 -> erase from starting_erasepage n pages (first 256 blocks is bootloader)\n");
          printf("-s startingpage -w value -> write blocks from ?.bin\n");
          printf("-f filename to use\n");
          printf("-E enable ECC\n\n");
          printf("Example:\n");
          printf("Write on first eraseblock of 64 * 2048 pages -> 0x00000-0x20000 == 131072 bytes == 128 kB\n");
          printf("./nandread_FTDI232H -p 0 -e 1\n");
          printf("./nandread_FTDI232H -f CP1416RAUGG-onlybootp.bin -s 0 -w 64\n");
          printf("./nandread_FTDI232H -f bootloader.bin -s 0 -r 64\n");

          printf("Write on second eraseblock (eripv2) -> 0x20000-0x40000 == 131072 bytes == 128 kB\n");
          printf("./nandread_FTDI232H -p 1 -e 1\n");
          printf("./nandread_FTDI232H -f eripv2.bin -s 64 -w 64\n");
          printf("./nandread_FTDI232H -f eripv2.bin -s 64 -r 64\n");

          printf("Write on third eraseblock 0x40000-0x80000 (rawstorage, keys) of 128 * 2048 pages -> 256 kB\n");
          printf("./nandread_FTDI232H -p 2 -e 2\n");
          printf("./nandread_FTDI232H -f rawstorage.bin -s 128 -r 128\n");

          printf("erase bank1 0x2000000-0x4500000\n");
          printf("./nandread_FTDI232H -p 256 -e 296\n");
          printf("Write on bank1 0x2000000-0x4500000\n");
          printf("./nandread_FTDI232H -f bank1.bin -s 16384 -w 18944\n");

          printf("erase bank2 0x4500000-0x6a00000\n");
          printf("./nandread_FTDI232H -p 552 -e 296\n");
          printf("Write on bank2 0x4500000-0x6a00000\n");
          printf("./nandread_FTDI232H -f bank2.bin -s 35328 -r 18944\n");
          //bank2 start 35328 total 18944  -> ends at 54272
          printf("./nandread_FTDI232H -f a.bin -s 54270 -r 2\n");
          printf("./nandread_FTDI232H -f CP1504SAL7M-last_bank2_512pages.bin -s 53760 -w 512\n");
          

          printf("check secondary kernel @ 4700000 \n");
          printf("./nandread_FTDI232H -s 36352 -r nnn\n");

          printf("check userfs @ 0x80000 - 0x2000000\n");
          printf("./nandread_FTDI232H -s 256 -r nnn\n");
          
          
          }
        }
    }

    // set nCE high
    controlbus_pin_set(PIN_nCE, HIGH);

    ftdi_usb_close(pBusInterface);
    ftdi_free(pBusInterface);
    printf("Done");

    return 0;
}


	/* Erase all blocks */
    /* Erase block #0 */
//	erase_block(0);
//
//	usleep(1* 1000000);
//
//	/* Write pages 0..9 with dummy data */

