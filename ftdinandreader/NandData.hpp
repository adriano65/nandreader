#ifndef NANDDATA_HPP
#define NANDDATA_HPP

#include "FtdiNand.hpp"
#include "FtdiNand_BB.hpp"
#include "NandID.hpp"
#include "NandCmds.h"

using namespace std;

class NandData {
public:
  #ifdef BITBANG_MODE
	NandData(FtdiNand_BB *, NandID *);
  #else
	NandData(FtdiNand *, NandID *);
  #endif
	~NandData();
	int readPage(unsigned int, unsigned char *buf, int len);
	int writePage(unsigned int, unsigned char *buf, int len);
	int erasePage(unsigned int );
	int readPageSP(unsigned long, unsigned char *buf);
	int writePageSP(unsigned long, unsigned char *buf, int len);
	int erasePageSP(unsigned int );
protected:
  #ifdef BITBANG_MODE
	FtdiNand_BB *pFtdiNand;
  #else
	FtdiNand *pFtdiNand;
  #endif
	NandID *pNandID;
};

#endif
