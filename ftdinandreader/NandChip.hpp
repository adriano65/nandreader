#ifndef NANDCHIP_HPP
#define NANDCHIP_HPP

#include "FtdiNand_BB.hpp"
#include "FtdiNand.hpp"
#include "NandID.hpp"
#include "NandData.hpp"

using namespace std;

class NandChip {
public:
  enum Action {
    actionNone=0,
    actionRead,
    actionWrite,
    actionVerify,
    actionErase
  };

	enum AccessType {
		None,
    NandPageSz_eq_FilePageSz,
    NandPagewOOBSz_eq_FilePagewOOBSz,
		NandPageSz_to_FilePagewRecalcOOBSz,   // only read
    FilePage_to_NandPagewOOBAddedSz,      // only write
    NandPagewOOBSz_to_FileOOBOnly,        // only read
		useBitBang,
	  };
  NandChip(int vid, int pid, bool doSlow, AccessType _accessType, Action action, unsigned long _start_address, unsigned long _end_address);
	~NandChip();
  AccessType accessType;
  unsigned long start_address;
  unsigned long end_address;
  unsigned int nandPageSize;
  unsigned int filePageSize;
  unsigned int fileOffset;
  unsigned int erasepageSize;
  unsigned int start_erasepageno;
  unsigned int end_erasepageno;
  unsigned char *pageBuf;

	enum AddressCheck {
		OK=0,
		BadEraseStart=1,
		BadStart=2,
		BadEraseEnd=3,
		BadEnd=4,
	};
  void setSizes(AccessType);
  int open();
	int readPage(unsigned long );
	int writePage(unsigned long );
	int erasePage(unsigned int );
	NandID *getIdPtr();
private:
	FtdiNand *pFtdiNand;
	NandID *pNandID;
	NandData *pNandData;
  unsigned int start_pageno;
  unsigned int end_pageno;
  void shift_half_byte(const uint8_t *src, uint8_t *dest, size_t sz);
  int brcm_nand(unsigned poly, unsigned char *data);
};

#endif