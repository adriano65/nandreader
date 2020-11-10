#ifndef FTDINAND_BB
#define FTDINAND_BB

#include <libftdi1/ftdi.h>
#include "FtdiNand.hpp"

using namespace std;

class FtdiNand_BB : public FtdiNand {
public:
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
#define PIN_nCE  0x01
#define PIN_nWP  0x02
#define PIN_nRE  0x04
#define PIN_nWE  0x08
#define PIN_RDY  0x10
#define PIN_ALE  0x20
#define PIN_CLE  0x40
//#define PIN_nWP  0x80
#define CONTROLBUS_BITMASK 0b01111111

#define STATUSREG_IO0  0x01

#define REALWORLD_DELAY 10 /* 10 usec */
//#define REALWORLD_DELAY 300
//#define REALWORLD_DELAY 1






	FtdiNand_BB();
	~FtdiNand_BB();
  int open(int _vid, int _pid, bool _doslow) override;
	int sendCmd(unsigned char cmd) override;
	int sendAddr(unsigned int addr, int noBytes);
	int readData(unsigned char *data, int count) override;
	int writeData(unsigned char *data, int count);
	int waitReady();
	unsigned char status();

private:
  typedef enum { LOW=0, HIGH=1 } onoff_t;
  typedef enum { IOBUS_IN=0, IOBUS_OUT=1 } iobus_inout_t;
  void EnableRead(bool bEnable);


	int error(const char *err);

  void controlbus_pin_set(unsigned char pin, onoff_t val);
  void controlbus_update_output();
  void iobus_set_direction(iobus_inout_t inout);
  void iobus_update_output();
  unsigned char controlbus_read_input();
  int latch_command(unsigned char *command, unsigned char count);

	int nandRead(int cl, int al, unsigned char *buf, int count);
	int nandWrite(int cl, int al, unsigned char *buf, int count);
	struct ftdi_context *pBusInterface;
	bool m_slowAccess;
	int m_rbErrorCount;

  unsigned char iobus_value;
  unsigned char controlbus_dir, controlbus_value;
  unsigned char crtl_buf[4], io_buf[4];
  unsigned char column_addr[2], row_addr[3];

};

#endif
