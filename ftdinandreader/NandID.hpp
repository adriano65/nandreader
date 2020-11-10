#ifndef NANDID_HPP
#define NANDID_HPP

#include <string>
using namespace std;

class NandID {
public:
	NandID(unsigned char *idbytes);
	~NandID();
	string getManufacturer();
	string getDesc();
	unsigned int getPageSize();
	unsigned int getSizeMB();
	unsigned int getOobSize();
  unsigned int getEraseSz();
	bool isLargePage();
	int getAddrByteCount();
  int getEraseAddrByteCount();
  unsigned char blockSz;

private:
	typedef struct {
		const char *name;
		unsigned char id;
		unsigned int pagesize;
		unsigned int chipsizeMB;
		unsigned int erasesize;
		unsigned char addrByteCount;
		unsigned char eraseAddrByteCount;
		int options;
	  } DevCodes;
	static const DevCodes m_devCodes[];
	char m_idBytes[5];

	string m_nandManuf;
	string m_nandDesc;
	int m_nandPageSz;
	int m_nandOobSz;
	int m_nandEraseSz;
	int m_nandChipSzMB;
	bool m_nandIsLP;
  unsigned char addrByteCount;
  unsigned char eraseAddrByteCount;
};

#endif