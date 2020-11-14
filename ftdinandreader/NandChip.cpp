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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h> // memcpy

#include "NandChip.hpp"
#include "NandData.hpp"
#include "NandCmds.h"
#include "bch.h"

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

NandChip::NandChip(int vid, int pid, bool doSlow, AccessType _accessType, Action action, unsigned long _start_address, unsigned long _end_address) {
	unsigned char id[5];

  accessType=_accessType;
  if (accessType==useBitBang) {
	  pFtdiNand=new FtdiNand_BB();
    }
  else
	  pFtdiNand=new FtdiNand();

  pFtdiNand->open(vid, pid, doSlow);
  
	pFtdiNand->sendCmd(NAND_CMD_RESET);
	pFtdiNand->waitReady();

	pFtdiNand->sendCmd(NAND_CMD_READID);
	pFtdiNand->sendAddr(NAND_ADDR_READID1, 1);
	pFtdiNand->readData(id, 5);

	pNandID=new NandID(id);
  pNandData=new NandData(pFtdiNand, pNandID);

	pFtdiNand->sendCmd(NAND_CMD_GETFEATURES);
	pFtdiNand->sendAddr(NAND_ADDR_TIMINGS, 1);
	pFtdiNand->readData(id, 5);
  DBG_MIN("NAND_CMD_GETFEATURES->NAND_ADDR_TIMINGS 0x%02X 0x%02X 0x%02X 0x%02X\n", id[0], id[1], id[2], id[3]);
	pFtdiNand->sendCmd(NAND_CMD_GETFEATURES);
	pFtdiNand->sendAddr(NAND_ADDR_ARRAYOP, 1);
	pFtdiNand->readData(id, 5);
  DBG_MIN("NAND_CMD_GETFEATURES->NAND_ADDR_ARRAYOP 0x%02X 0x%02X 0x%02X 0x%02X\n", id[0], id[1], id[2], id[3]);

  fileOffset=0;
  erasepageSize=pNandID->getEraseSz();
  start_address=_start_address;
  if (_end_address == 0) { 
    end_address = (pNandID->getSizeMB()*1024L*1024L); 
    end_pageno=(pNandID->getSizeMB()*1024L*1024L)/pNandID->getPageSize();
    }
  else {
    end_address=_end_address;
    }

  switch (action) {
    // loads pages from file (withot OOB), calc OOB and write to nand adding OOB data
    case actionRead:
      setSizes(accessType);
      if (start_address%nandPageSize == 0) { start_pageno=start_address/nandPageSize; }
      else { 
        DBG_MIN("Misaligned start_address for R/W: use 0x%08X", (unsigned long)(start_address/nandPageSize)*nandPageSize); 
        exit(1);
        }

      if (end_address%nandPageSize == 0) { end_pageno=end_address/nandPageSize; }
      else { 
        DBG_MIN("Misaligned end_address for R/W: use 0x%08X", (unsigned long)(end_address/nandPageSize)*nandPageSize);
        exit(1);
        }
      break;
    case actionWrite:
      setSizes(accessType);
      if (start_address%nandPageSize == 0) { start_pageno=start_address/nandPageSize; }
      else { 
        DBG_MIN("Misaligned start_address for R/W: use 0x%08X", (unsigned long)(start_address/nandPageSize)*nandPageSize); 
        exit(1);
        }

      if (end_address%nandPageSize == 0) { end_pageno=end_address/nandPageSize; }
      else { 
        DBG_MIN("Misaligned end_address for R/W: use 0x%08X", (unsigned long)(end_address/nandPageSize)*nandPageSize);
        exit(1);
        }
      break;

    case actionErase:
      if (start_address%erasepageSize == 0) { start_erasepageno=start_address/erasepageSize; }
      else {
        DBG_MIN("Misaligned start_address for erasing: use 0x%08X", (unsigned long)(start_address/erasepageSize)*erasepageSize);
        exit(1);
        }
      if (end_address%erasepageSize == 0) { end_erasepageno=end_address/erasepageSize; }
      else {
        DBG_MIN("Misaligned end_address for erasing: use 0x%08X", (unsigned long)(end_address/erasepageSize)*erasepageSize);
        exit(1);
        }
      break;
    }

  }

NandChip::~NandChip() {
	delete pFtdiNand;
  //delete[] pNandData;
	//delete pNandID;
}

void NandChip::setSizes(AccessType accessType) {
  erasepageSize=pNandID->getEraseSz();
  switch (accessType) {
    case NandPageSz_eq_FilePageSz:
      nandPageSize=filePageSize=pNandID->getPageSize();
      pageBuf=new unsigned char[filePageSize];
      break;

    case NandPagewOOBSz_eq_FilePagewOOBSz:
      nandPageSize=pNandID->getPageSize();
      filePageSize=nandPageSize+pNandID->getOobSize();
      pageBuf=new unsigned char[filePageSize];
      break;

    case NandPagewOOBSz_to_FileOOBOnly:
      nandPageSize=pNandID->getPageSize();
      filePageSize=nandPageSize+pNandID->getOobSize();
      pageBuf=new unsigned char[filePageSize];
      fileOffset=nandPageSize;
      break;

    case NandPageSz_to_FilePagewRecalcOOBSz:    //Only Read
      nandPageSize=pNandID->getPageSize();
      filePageSize=nandPageSize+pNandID->getOobSize();
      pageBuf=new unsigned char[filePageSize];
      break;

    case FilePage_to_NandPagewOOBAddedSz:       //Only Write
      nandPageSize=pNandID->getPageSize();
      filePageSize=nandPageSize+pNandID->getOobSize();
      pageBuf=new unsigned char[filePageSize];
      break;

    default:
      DBG_MIN("Wrong case");
      exit(0);
      break;
    }
}

int NandChip::readPage(unsigned long address) {
  int nRes;
  unsigned int row;
  row=address/nandPageSize;
  switch (accessType) {
    // each page of nand is loaded and written to file without OOB
    case NandPageSz_eq_FilePageSz:
      nRes=pNandData->readPage(row, pageBuf, nandPageSize);;
      break;

    // each page+oob size of nand is loaded and page+oob are written to file
    case NandPagewOOBSz_eq_FilePagewOOBSz:
    case NandPagewOOBSz_to_FileOOBOnly:
      nRes=pNandData->readPage(row, pageBuf, filePageSize);
      break;
    
    // each page size of nand is loaded, oob data is substituted with recalculated oob and page+oob page bytes are written to file
    case NandPageSz_to_FilePagewRecalcOOBSz:
      memset(pageBuf, 0xFF, filePageSize); // prepare buff for bch
      pNandData->readPage(row, pageBuf, nandPageSize);
      brcm_nand(0, pageBuf);                //default polynomial
      //brcm_nand(8, pageBuf);                // NO
      //brcm_nand(0x25, pageBuf);             // NO
      nRes=filePageSize;
      break;

    // we are called from Write to check :-)
    case FilePage_to_NandPagewOOBAddedSz:
      nRes=pNandData->readPage(row, pageBuf, filePageSize);
      break;


    default:
      DBG_MIN("unexpected case");
      exit(0);
      break;
    }
	return nRes;
}

int NandChip::writePage(unsigned long address) {
  int nRes;
  unsigned int row;
  row=address/nandPageSize;
  switch (accessType) {
    // each page of file is loaded of nand page size and written to nand
    case NandPageSz_eq_FilePageSz:
      nRes=pNandData->writePage(row, pageBuf, nandPageSize);;
      break;
    
    // each page of file is loaded of nand page+oob size and written to nand
    case NandPagewOOBSz_eq_FilePagewOOBSz:
      nRes=pNandData->writePage(row, pageBuf, filePageSize);
      break;

    // each page of file is loaded of page size, recalculated oob data is added and written to nand
    case FilePage_to_NandPagewOOBAddedSz:
      //brcm_nand(0, pageBuf);              //default polynomial --> NO
      brcm_nand(0x25, pageBuf);
      pNandData->writePage(row, pageBuf, filePageSize);
      nRes=filePageSize;
      break;

    default:
      DBG_MIN("unexpected case");
      exit(0);
      break;
    }
	return nRes;
}

int NandChip::erasePage(unsigned int page) {
  int nRet;
	nRet=pNandData->erasePage(page);
	return nRet;
}

NandID *NandChip::getIdPtr() {
	return pNandID;
}


/*
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
  usleep(REALWORLD_DELAY);
  controlbus_pin_set(PIN_nWE, HIGH);  // toggle nWE back high (acts as clock to latch the current address byte!)
  controlbus_update_output();
  usleep(REALWORLD_DELAY);
  controlbus_pin_set(PIN_ALE, LOW);
  controlbus_update_output();


  latch_data_out(data, 4);

  return 1;
}
*/


/**
 *
 * This program reads raw NAND image from standard input and updates ECC bytes in the OOB block for each sector.
 * Data layout is as following:
 *
 * 2 KB page, consisting of 4 x 512 B sectors
 * 64 bytes OOB, consisting of 4 x 16 B OOB regions, one for each sector
 *
 * In each OOB region, the first 9 1/2 bytes are user defined and the remaining 6 1/2 bytes are ECC.
 *
 */

#define BCH_T 4
#define BCH_N 13
#define SECTOR_SZ 512
#define OOB_SZ 16
#define SECTORS_PER_PAGE 4
#define OOB_ECC_OFS 9
#define OOB_ECC_LEN 7

// Wide right shift by 4 bits. Preserves the very first 4 bits of the output.
void NandChip::shift_half_byte(const uint8_t *src, uint8_t *dest, size_t sz){
	// go right to left since input and output may overlap
	unsigned j;
	dest[sz] = src[sz - 1] << 4;
	for (j = sz; j != 0; --j)
		dest[j] = src[j] >> 4 | src[j - 1] << 4;
	dest[0] |= src[0] >> 4;
}

int NandChip::brcm_nand(unsigned poly, unsigned char *data) {
	struct bch_control *bch = init_bch(BCH_N, BCH_T, poly);
	if (!bch)
		return -1;

	uint8_t page_buffer[(SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE];

  memcpy(page_buffer, data, (SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE);
  // Erased pages have ECC = 0xff .. ff even though there may be user bytes in the OOB region
  int erased_block = 1;
  unsigned i;
  for (i = 0; i != SECTOR_SZ * SECTORS_PER_PAGE; ++i)
    if (page_buffer[i] != 0xff)	{
      erased_block = 0;
      break;
    }

  for (i = 0; i != SECTORS_PER_PAGE; ++i)	{
    const uint8_t *sector_data = page_buffer + SECTOR_SZ * i;
    uint8_t *sector_oob = page_buffer + SECTOR_SZ * SECTORS_PER_PAGE + OOB_SZ * i;
    if (erased_block)	{
      // erased page ECC consumes full 7 bytes, including high 4 bits set to 0xf
      memset(sector_oob + OOB_ECC_OFS, 0xff, OOB_ECC_LEN);
      }
    else	{
      // concatenate input data
      uint8_t buffer[SECTOR_SZ + OOB_ECC_OFS + 1];
      buffer[0] = 0;
      shift_half_byte(sector_data, buffer, SECTOR_SZ);
      shift_half_byte(sector_oob, buffer + SECTOR_SZ, OOB_ECC_OFS);
      // compute ECC
      uint8_t ecc[OOB_ECC_LEN];
      memset(ecc, 0, OOB_ECC_LEN);
      encode_bch(bch, buffer, SECTOR_SZ + OOB_ECC_OFS + 1, ecc);
      // copy the result in its OOB block, shifting right by 4 bits
      shift_half_byte(ecc, sector_oob + OOB_ECC_OFS, OOB_ECC_LEN - 1);
      sector_oob[OOB_ECC_OFS + OOB_ECC_LEN - 1] |= ecc[OOB_ECC_LEN - 1] >> 4;
      }
    }
  memcpy(data, page_buffer, (SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE);

  return 0;
}
