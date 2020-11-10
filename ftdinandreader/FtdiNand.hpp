#ifndef FTDINAND
#define FTDINAND

#include <libftdi1/ftdi.h>

using namespace std;

class FtdiNand {
public:
	FtdiNand();
	~FtdiNand();
  virtual int open(int _vid, int _pid, bool _doslow);
  void EnableRead(bool bEnable);
  void DisableACBUS(bool bEnable);
	virtual int sendCmd(unsigned char cmd);
	virtual int sendAddr(unsigned int, int);
	int writeData(unsigned char *data, int count);
	virtual int readData(unsigned char *data, int count);
	int waitReady();
	unsigned char status();
private:
	int error(const char *err);
	int nandRead(int cl, int al, unsigned char *buf, int count);
	int nandWrite(int cl, int al, unsigned char *buf, int count);
	int nandRead_ori(int cl, int al, unsigned char *buf, int count);
	int nandWrite_ori(int cl, int al, unsigned char *buf, int count);
	struct ftdi_context m_ftdi;
  int vid;
  int pid;
	bool m_slowAccess;
	int m_rbErrorCount;  
};

#endif
