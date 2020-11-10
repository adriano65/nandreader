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


#include "NandID.hpp"
#include <stdio.h>
#include <stdlib.h>

#define LP_OPTIONS 1

//Table is 'borrowed' from the Linux kernel: drivers/mtd/nandids.c
//Copyright (C) 2002 Thomas Gleixner (tglx@linutronix.de)
//Modified to get rid of the 16-bit devices: we don't support these.
//Name. ID code, pagesize, chipsize in MegaByte, eraseblock size, addrByteCount, eraseAddrByteCount options
const NandID::DevCodes NandID::m_devCodes[]={
	{"NAND 2MiB 5V 8-bit",		0x64, 256, 2, 0x1000, 0, 0, 0},
	{"NAND 1MiB 5V 8-bit",		0x6e, 256, 1, 0x1000, 0, 0, 0},
	{"NAND 4MiB 5V 8-bit",		0x6b, 512, 4, 0x2000, 0, 0, 0},
	{"NAND 1MiB 3,3V 8-bit",	0xe8, 256, 1, 0x1000, 0, 0, 0},
	{"NAND 1MiB 3,3V 8-bit",	0xec, 256, 1, 0x1000, 0, 0, 0},
	{"NAND 2MiB 3,3V 8-bit",	0xea, 256, 2, 0x1000, 0, 0, 0},
	{"NAND 4MiB 3,3V 8-bit",	0xd5, 512, 4, 0x2000, 0, 0, 0},
	{"NAND 4MiB 3,3V 8-bit",	0xe3, 512, 4, 0x2000, 0, 0, 0},
	{"NAND 4MiB 3,3V 8-bit",	0xe5, 512, 4, 0x2000, 0, 0, 0},
	{"NAND 8MiB 3,3V 8-bit",	0xd6, 512, 8, 0x2000, 0, 0, 0},

	{"NAND 8MiB 1,8V 8-bit",	0x39, 512, 8, 0x2000, 0, 0, 0},
	{"NAND 8MiB 3,3V 8-bit",	0xe6, 512, 8, 0x2000, 0, 0, 0},

	{"NAND 16MiB 1,8V 8-bit",	0x33, 512, 16, 0x4000, 0, 0, 0},
	{"NAND 16MiB 3,3V 8-bit",	0x73, 512, 16, 0x4000, 0, 0, 0},

	{"NAND 32MiB 1,8V 8-bit",	0x35, 512, 32, 0x4000, 0, 0, 0},
	{"NAND 32MiB 3,3V 8-bit",	0x75, 512, 32, 0x4000, 0, 0, 0},

	{"NAND 64MiB 1,8V 8-bit",	0x36, 512, 64, 0x4000, 0, 0, 0},
	{"NAND 64MiB 3,3V 8-bit",	0x76, 512, 64, 0x4000, 0, 0, 0},

	{"NAND 128MiB 1,8V 8-bit",	0x78, 512, 128, 0x4000, 0, 0, 0},
	{"NAND 128MiB 1,8V 8-bit",	0x39, 512, 128, 0x4000, 0, 0, 0},
	{"NAND 128MiB 3,3V 8-bit",	0x79, 512, 128, 0x4000, 0, 0, 0},

	{"NAND 256MiB 3,3V 8-bit",	0x71, 512, 256, 0x4000, 0, 0, 0},

	/* 512 Megabit */
	{"NAND 64MiB 1,8V 8-bit",	0xA2, 0,  64, 0, 0, 0, LP_OPTIONS},
	{"NAND 64MiB 1,8V 8-bit",	0xA0, 0,  64, 0, 0, 0, LP_OPTIONS},
	{"NAND 64MiB 3,3V 8-bit",	0xF2, 0,  64, 0, 0, 0, LP_OPTIONS},
	{"NAND 64MiB 3,3V 8-bit",	0xD0, 0,  64, 0, 0, 0, LP_OPTIONS},
	{"NAND 64MiB 3,3V 8-bit",	0xF0, 0,  64, 0, 0, 0, LP_OPTIONS},

	/* 1 Gigabit */
	{"NAND 128MiB 1,8V 8-bit",	0xA1, 0, 128, 0, 0, 0, LP_OPTIONS},
	//{"NAND 128MiB 3,3V 8-bit",	0xF1, 2048, 128, 131072, 4, 2, LP_OPTIONS},
	{"NAND 128MiB 3,3V 8-bit",	0xF1, 0, 128, 0, 0, 0, LP_OPTIONS},
	{"NAND 128MiB 3,3V 8-bit",	0xD1, 0, 128, 0, 0, 0, LP_OPTIONS},
	//{"NAND 128MiB 3,3V 8-bit",	0x2C, 2048, 128, 131072, 0, 0, 0},   //Micron
  //{"MT29F1G08ABADAWP", {0x2C, 0xF1, 0x80, 0x95, 0x02}, 2048, 64, 64, MT29},

	/* 2 Gigabit */
	{"NAND 256MiB 1,8V 8-bit",	0xAA, 0, 256, 0, 0, 0, LP_OPTIONS},
	{"NAND 256MiB 3,3V 8-bit",	0xDA, 0, 256, 0, 0, 0, LP_OPTIONS},

	/* 4 Gigabit */
	{"NAND 512MiB 1,8V 8-bit",	0xAC, 0, 512, 0, 0, 0, LP_OPTIONS},
	{"NAND 512MiB 3,3V 8-bit",	0xDC, 0, 512, 0, 0, 0, LP_OPTIONS},

	/* 8 Gigabit */
	{"NAND 1GiB 1,8V 8-bit",	0xA3, 0, 1024, 0, 0, 0, LP_OPTIONS},
	{"NAND 1GiB 3,3V 8-bit",	0xD3, 0, 1024, 0, 0, 0, LP_OPTIONS},

	/* 16 Gigabit */
	{"NAND 2GiB 1,8V 8-bit",	0xA5, 0, 2048, 0, 0, 0, LP_OPTIONS},
	{"NAND 2GiB 3,3V 8-bit",	0xD5, 0, 2048, 0, 0, 0, LP_OPTIONS},

	/* 32 Gigabit */
	{"NAND 4GiB 1,8V 8-bit",	0xA7, 0, 4096, 0, 0, 0, LP_OPTIONS},
	{"NAND 4GiB 3,3V 8-bit",	0xD7, 0, 4096, 0, 0, 0, LP_OPTIONS},

	/* 64 Gigabit */
	{"NAND 8GiB 1,8V 8-bit",	0xAE, 0, 8192, 0, 0, 0, LP_OPTIONS},
	{"NAND 8GiB 3,3V 8-bit",	0xDE, 0, 8192, 0, 0, 0, LP_OPTIONS},
	{"NAND 8GiB 3,3V 8-bit",	0xAD, 0, 8192, 0, 0, 0, LP_OPTIONS},

	/* 128 Gigabit */
	{"NAND 16GiB 1,8V 8-bit",	0x1A, 0, 16384, 0, 0, 0, LP_OPTIONS},
	{"NAND 16GiB 3,3V 8-bit",	0x3A, 0, 16384, 0, 0, 0, LP_OPTIONS},

	/* 256 Gigabit */
	{"NAND 32GiB 1,8V 8-bit",	0x1C, 0, 32768, 0, 0, 0, LP_OPTIONS},
	{"NAND 32GiB 3,3V 8-bit",	0x3C, 0, 32768, 0, 0, 0, LP_OPTIONS},

	/* 512 Gigabit */
	{"NAND 64GiB 1,8V 8-bit",	0x1E, 0, 65536, 0, 0, 0, LP_OPTIONS},
	{"NAND 64GiB 3,3V 8-bit",	0x3E, 0, 65536, 0, 0, 0, LP_OPTIONS},
	{"", 0, 0, 0, 0, 0, 0, 0}
};

//Constructor: construct NAND ID info from the 5 ID bytes read from the NAND.
NandID::NandID(unsigned char *idBytes) {
	int x;
	for (x=0; x<5; x++) m_idBytes[x]=idBytes[x];

	x=0;
	while (m_devCodes[x].id!=0 && m_devCodes[x].id!=idBytes[1]) x++;
	if (m_devCodes[x].id==0) {
		printf("Sorry, unknown nand chip with id code 0x%02X 0x%02X 0x%02X 0x%02X\n", idBytes[0], idBytes[1], idBytes[2], idBytes[3]);
		exit(0);
	  }
	m_nandDesc=m_devCodes[x].name;
	m_nandChipSzMB=m_devCodes[x].chipsizeMB;
	m_nandIsLP=((m_devCodes[x].options&LP_OPTIONS)!=0);
	if (m_devCodes[x].pagesize!=0) {
		//Page/erasesize is device-specific
		m_nandPageSz=m_devCodes[x].pagesize;
		m_nandOobSz=(m_nandPageSz==512)?16:64;       // bytes every 512
		m_nandEraseSz=m_devCodes[x].erasesize;
		addrByteCount=m_devCodes[x].addrByteCount;
		eraseAddrByteCount=m_devCodes[x].eraseAddrByteCount;
	  }
  else {
		//Page/erasesize is encoded in 3/4/5th ID-byte
		int i;
		i=idBytes[3]&3;
		m_nandPageSz=1024<<i;
		i=(idBytes[3]>>4)&3;
		m_nandEraseSz=(64*1024)<<i;
		m_nandOobSz=((idBytes[3]&4)?16:8)*(m_nandPageSz/512);
		eraseAddrByteCount=2;
    blockSz=m_nandEraseSz/m_nandPageSz;
	}

	char buff[100];
	sprintf(buff, "Unknown (%hhx)", idBytes[0]);
	m_nandManuf=buff;
	if (idBytes[0]==0x01) m_nandManuf="Cypress";
	if (idBytes[0]==0x04) m_nandManuf="Fujitsu";
	if (idBytes[0]==0x07) m_nandManuf="Renesas";
	if (idBytes[0]==0x20) m_nandManuf="ST Micro";
	if (idBytes[0]==0x98) m_nandManuf="Toshiba";
	if (idBytes[0]==0xec) m_nandManuf="Samsung";
	if (idBytes[0]==0x8f) m_nandManuf="National Semiconductors";
	if (idBytes[0]==0xad) m_nandManuf="Hynix";
	if (idBytes[0]==0x2c) m_nandManuf="Micron";
	if (idBytes[0]==0xc2) m_nandManuf="Macronix";
	if (idBytes[0]==0xf1) m_nandManuf="Samsung2";

  printf("Nand ID: Manufactor 0x%02X, Type 0x%02X, 0x%02X, 0x%02X\n", idBytes[0], idBytes[1], idBytes[2]);
  printf("Type: %s, Manufacturer: %s\n", getDesc().c_str(), getManufacturer().c_str());
  printf("Size: %iMiB, pagesize %i bytes, OOB size %i bytes, erasepage size %i\n", getSizeMB(), getPageSize(), getOobSize(), getEraseSz());
  printf( "full pagesize %i bytes, full erasepage size %i\n", getPageSize()+getOobSize(), getEraseSz()+(getOobSize()*64) );
  printf("%s page, needs %i addr bytes.\n", isLargePage()?"Large":"Small", getAddrByteCount());	
}

NandID::~NandID() {
  ;
}

string NandID::getDesc() {
	return m_nandDesc;
}

string NandID::getManufacturer() {
	return m_nandManuf;
}

unsigned int NandID::getPageSize() {
	return m_nandPageSz;
}

unsigned int NandID::getOobSize() {
	return m_nandOobSz;
}

unsigned int NandID::getSizeMB() {
	return m_nandChipSzMB;
}

unsigned int NandID::getEraseSz() {
	return m_nandEraseSz;
}

bool NandID::isLargePage() {
	return m_nandIsLP;
}

//Get the amount of bytes the address needs to be sent as.
int NandID::getAddrByteCount() {
	int cyc;
  //if ( addrByteCount ) return addrByteCount;

	if (m_nandIsLP) {
		if (m_nandChipSzMB>=32768) {
			cyc=6;
		  } 
    else if (m_nandChipSzMB>=128) {
			      cyc=5;
		        }
         else {
			        cyc=4;
		          }
	  }
  else {
		if (m_nandChipSzMB<=64) {
			cyc=3; 
		  }
    else {
			cyc=4;
		  }
	}
	return cyc;
}
// S34ML01G1
int NandID::getEraseAddrByteCount() {
  if (eraseAddrByteCount) return eraseAddrByteCount;
	return 0;
}


