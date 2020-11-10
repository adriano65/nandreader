/*
Software to interface to a NAND-flash chip connected to a FT2232H IC.
(C) 2012 Jeroen Domburg (jeroen AT spritesmods.com)

This program is free software: you can redistribute it and/or modify
t under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "FtdiNand.hpp"
#include "FtdiNand_BB.hpp"
#include "NandCmds.h"

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


//Masks for the address lines (for addrh) the various nand latch lines are connected to.
#define ADR_WP 0x20     // ACBUS5   - Nand Pin 19 Active low --> Write protect Active
#define ADR_CL 0x40     // ACBUS6
#define ADR_AL 0x80     // ACBUS7

FtdiNand_BB::FtdiNand_BB() {
	m_rbErrorCount=0;
  controlbus_dir=0b01101111;
}

//Destructor: Close everything.
FtdiNand_BB::~FtdiNand_BB(void) {
  EnableRead(false);
	ftdi_usb_close(pBusInterface);
	ftdi_deinit(pBusInterface);
}

int FtdiNand_BB::open(int _vid, int _pid, bool _doslow) {
  int f;
  unsigned int FTDIchipID;
  if ((pBusInterface = ftdi_new()) == 0) { error("ftdi_new fail"); }
  ftdi_set_interface(pBusInterface, INTERFACE_A);
  if ( (f=ftdi_usb_open(pBusInterface, 0x0403, 0x6010)) != 0 ) {
      fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f, ftdi_get_error_string(pBusInterface));
      ftdi_free(pBusInterface);
      exit(-1);
    }
  pBusInterface->bitbang_enabled=0;

  ftdi_read_chipid(pBusInterface, &FTDIchipID);
  printf("ftdi_read_chipid 0x%04X\n", FTDIchipID);

  ftdi_usb_reset(pBusInterface);
  ftdi_set_interface(pBusInterface, INTERFACE_ANY);
  ftdi_set_bitmode(pBusInterface, 0, 0); // reset
  ftdi_set_bitmode(pBusInterface, 0, BITMODE_MPSSE);
  ftdi_usb_purge_buffers(pBusInterface);
  usleep(50000); // sleep 50 ms for setup to complete

  EnableRead(true);
}

//Error handling routine.
int FtdiNand_BB::error(const char *err) {
	printf("Error at %s: %s\n\n", err, ftdi_get_error_string(pBusInterface));
	exit(0); //Dirty. Disable to continue after errors.
	return 0;
}

void FtdiNand_BB::controlbus_pin_set(unsigned char pin, onoff_t val) {
    if(val == HIGH)
        controlbus_value |= pin;
    else
        controlbus_value &= (unsigned char)0xFF ^ pin;
}

void FtdiNand_BB::controlbus_update_output() {
    crtl_buf[0] = SET_BITS_HIGH;
    crtl_buf[1] = controlbus_value;       // pinState
    crtl_buf[2] = controlbus_dir;         // pinDirection 0=in, 1=out
    if ( ftdi_write_data(pBusInterface, crtl_buf, 3) < 0 ) printf("ftdi_write_data fail\n");
}

// nand data bus - ADBUS
void FtdiNand_BB::iobus_set_direction(iobus_inout_t inout) {
    io_buf[0] = SET_BITS_LOW;
    io_buf[1] = 0xFF;       // pinState
    io_buf[2] = (inout == IOBUS_OUT) ? 0xFF : 0x00;       // pinDirection 0=in, 1=out
    if ( ftdi_write_data(pBusInterface, io_buf, 3) < 0 ) printf("ftdi_write_data fail\n");
}

// nand data bus - ADBUS
void FtdiNand_BB::iobus_update_output() {
    io_buf[0] = SET_BITS_LOW;
    io_buf[1] = iobus_value;  // pinState
    io_buf[2] = 0xFF;         // pinDirection 0=in, 1=out
    if ( ftdi_write_data(pBusInterface, io_buf, 3) < 0 ) printf("ftdi_write_data fail\n");
}

unsigned char FtdiNand_BB::controlbus_read_input() {
    unsigned char buf;
    ftdi_read_pins(pBusInterface, &buf);
    return buf;
}


/* Command Input bus operation is used to give a command to the memory device. Command are accepted with Chip
Enable low, Command Latch Enable High, Address Latch Enable low and Read Enable High and latched on the rising
edge of Write Enable. Moreover for commands that starts a modify operation (write/erase) the Write Protect pin must be
high. */
int FtdiNand_BB::latch_command(unsigned char *command, unsigned char count) {
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






//Read bytes from the nand, with the cl and al lines set as indicated.
int FtdiNand_BB::nandRead(int cl, int al, unsigned char *buf, int count) {
	unsigned char *cmds=new unsigned char[count*2+2];
	unsigned char *ftdata=new unsigned char[count*2];
	int x, i, bytesread;
	i=0;
	//Construct read commands. First one sets the cl and al lines too, rest just reads.
	for (x=0; x<count; x++) {
		if (x==0) {
			cmds[i++]=READ_EXTENDED;
			cmds[i++]=0xFF & ((cl?ADR_CL:0) | (al?ADR_AL:0) | ADR_WP);
			cmds[i++]=00;
		} else {
			cmds[i++]=READ_SHORT;
			cmds[i++]=0;
		}
	}
	cmds[i++]=SEND_IMMEDIATE;

//	printf("Cmd:\n");
//	for (x=0; x<i; x++) printf("%02hhx %s", cmds[x], ((x&15)==15)?"\n":"");
//	printf("\n\n");

	if (ftdi_write_data(pBusInterface, cmds, i)<0) return error("writing cmd");
	if (m_slowAccess) {
		//Div by 5 mode makes the ftdi-chip return all databytes double. Compensate for that.
		bytesread=ftdi_read_data(pBusInterface, ftdata, count*2);
		for (x=0; x<count; x++) buf[x]=ftdata[x*2];
		bytesread/=2;
	  }
  else {
		bytesread=ftdi_read_data(pBusInterface, ftdata, count);
		for (x=0; x<count; x++) 
      buf[x]=ftdata[x];
	}

	//if (ftdi_usb_purge_buffers(pBusInterface)<0) return error("ftdi_usb_purge_buffers");

	if (bytesread<0) return error("reading data");
	if (bytesread<count) return error("short read");
	//printf("%i bytes read.\n", bytesread); for (x=0; x<bytesread; x++) printf("0%i 0x%02X\n", x, ftdata[x]);
	delete[] cmds;
	delete[] ftdata;
	return bytesread;
}

//Write bytes to the nand, with al/cl set as indicated.
int FtdiNand_BB::nandWrite(int cl, int al, unsigned char *buf, int count) {
	unsigned char *cmds=new unsigned char[count*3+1];
	int x, i, byteswritten;
	i=0;

	//Construct write commands. First one sets the cl and al lines too, rest just reads.
	for (x=0; x<count; x++) {
		if (x==0) {
			cmds[i++]=WRITE_EXTENDED;
			cmds[i++] = 0xFF & ((cl ? ADR_CL : 0) | (al ? ADR_AL : 0) | ADR_WP);   // Address High ( ACBUS )
			cmds[i++]=00;       // Address low ( ADBUS )
		  }
    else {
			cmds[i++]=WRITE_SHORT;
			cmds[i++]=00;      // Address low ( ADBUS )
		  }
		cmds[i++]=buf[x];             // Data ( ADBUS )
	}

//	printf("Cmd:\n");
//	for (x=0; x<i; x++) printf("%02hhx %s", cmds[x], ((x&15)==15)?"\n":"");
//	printf("\n\n");

	if ((byteswritten=ftdi_write_data(pBusInterface, cmds, i))<0) {error("writing cmds"); }
	printf("byteswritten: %i\n", byteswritten);

	delete[] cmds;

	return count;
}

// BDBUS7 is on bit1 of byte (I/O 1)!!
// BDBUS6 is on bit0 of byte (I/O 0)!!
void FtdiNand_BB::EnableRead(bool bEnable) {
  // BDBUS0 --> nc   FTDI #CS
  // BDBUS1 --> nc   FTDI ALE
  // BDBUS2 --> #RE  read enable
  // BDBUS3 --> #WE  write enable
  // BDBUS4 --> nc   FTDI IORDY
  // BDBUS6 --> #CE  FTDI I/O0 pin 45
  // BDBUS7 --> R/#B  -- input Ready or #Busy
	unsigned char crtl_buf[3];
  crtl_buf[0] = SET_BITS_HIGH;   // BDBUS
  crtl_buf[1] = bEnable ? 0b11111110 : 0x7F;    // pinState
  crtl_buf[2] = bEnable ? 0x01 : 0x00;          // pinDirection 0=in, 1=out   -- only R/#B is input
  if ( ftdi_write_data(pBusInterface, crtl_buf, 3) < 0 ) error("SET_BITS_HIGH fail");
  usleep(10000);
}

unsigned char FtdiNand_BB::status() {
	unsigned char status;
	sendCmd(NAND_CMD_STATUS);
	readData((unsigned char*)&status, 1);
	return status;
}

//Send a command to the NAND chip
int FtdiNand_BB::sendCmd(unsigned char cmd) {
  iobus_set_direction(IOBUS_IN);
  // set nRE high and nCE and nWP low
  controlbus_pin_set(PIN_nRE, HIGH);
  controlbus_pin_set(PIN_nCE, LOW);
  controlbus_pin_set(PIN_nWP, LOW); /* When WP# is LOW, PROGRAM and ERASE operations are disabled */
  controlbus_update_output();
  latch_command(&cmd, 1);
}

//Send an address to the NAND. addr is the address and it is send
//as noBytes bytes. (the amount of bytes differs between flash types.)
int FtdiNand_BB::sendAddr(unsigned int addr, int noBytes) {
	unsigned char buff[10];
	int x;
  switch (noBytes) {
  case 1:
    return 0;
  case 2:
    buff[0]= addr&0xFF;
    buff[1]=(addr&0x00F00)>>8;
    //buff[1]= addr&0xFF;
    //buff[0]=(addr&0x00F00)>>8;
    break;
  
  case 4:
    buff[0]= addr&0xFF;
    buff[1]=(addr&0x00F00)>>8;
    buff[2]=(addr&0xFF000)>>12;
    buff[3]=(addr&0xFF00000)>>20;
    //buff[3]= addr&0xFF;
    //buff[2]=(addr&0x00F00)>>8;
    //buff[1]=(addr&0xFF000)>>12;
    //buff[0]=(addr&0xFF00000)>>20;
    break;

  default:
    for (x=0; x<noBytes; x++) {
      buff[x]=addr&0xff;
      addr=addr>>8;
      }
    break;
    }
	return 0;
}

//Write data to the flash.
int FtdiNand_BB::writeData(unsigned char *data, int count) {
	return nandWrite(0, 0, data, count);
}

//Read data from the flash.
int FtdiNand_BB::readData(unsigned char *data, int count) {
    return 0;
    unsigned int addr_idx = 0;
    int f;

    /* check if ALE is low and nRE is high */
    if( controlbus_value & PIN_nCE ) { fprintf(stderr, " requires nCE pin to be low\n"); return EXIT_FAILURE; }
    else if( ~controlbus_value & PIN_nWE ) { fprintf(stderr, " requires nWE pin to be high\n"); return EXIT_FAILURE; }
    else if( controlbus_value & PIN_ALE ) { fprintf(stderr, " requires ALE pin to be low\n"); return EXIT_FAILURE; }

    iobus_set_direction(IOBUS_IN);

    for(addr_idx = 0; addr_idx < count; addr_idx++) {
        /* toggle nRE low; acts like a clock to latch out the data; data is valid tREA after the falling edge of nRE 
         * (also increments the internal column address counter by one) */
        controlbus_pin_set(PIN_nRE, LOW);
        controlbus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure tREA delay */

        // read I/O pins
        if ((f=ftdi_read_pins(pBusInterface, &data[addr_idx])) < 0 ) { DBG_MIN("Err"); }

        // toggle nRE back high
        controlbus_pin_set(PIN_nRE, HIGH);
        controlbus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure tREH and tRHZ delays */
    }

    iobus_set_direction(IOBUS_OUT);
    return 0;
}

//Timeout in ms. Due to use of usleep(), not exact, but ballpark.
#define TIMEOUT_MSEC 100
#define TIMEOUT_RETRIES 15

//Waits till the R-/B-line to go high
int FtdiNand_BB::waitReady() {
	unsigned char cmd=GET_BITS_HIGH;
	unsigned char resp;
	int x;
	if (m_rbErrorCount==-1) return 1; //Too many timeouts -> don't check R/B-pin

	for (x=0; x<TIMEOUT_MSEC; x++) {
		//Send the command to read the IO-lines
		if (ftdi_write_data(pBusInterface, &cmd, 1)<0) { return error("writing cmd"); }
		//Read response
		if (ftdi_read_data(pBusInterface, &resp, 1)<0) { return error("reading resp"); }
		//Return if R/B-line is high (=ready)
		if (resp & 2) return 1;
		usleep(5000);
	}
	printf("Timeout on R/B-pin; chip seems busy for too long!\n");
	m_rbErrorCount++;
	if (m_rbErrorCount>TIMEOUT_RETRIES) {
		printf("WARNING: Too many R/B-pin timeouts. Going to ignore this pin for now.\n");
		printf("DOUBLE CHECK IF THE CHIP IS READ OR WRITTEN CORRECTLY!\n");
		m_rbErrorCount=-1;
	}
}
