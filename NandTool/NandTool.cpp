// NandTool.cpp : Definiert den Einstiegspunkt f�r die Konsolenanwendung.
//

#include <unistd.h>
#include <stdio.h>
#include "stdlib.h"
#include "string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h> 
#include "FtdiNand.hpp"
#include "NandChip.hpp"

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

void usage() {
		printf("Usage: [-i|-r file|-v file] [-p <start page no> <end pageno> [-t main|oob|both] [-s]\n");
		printf("  -i      - Identify chip\n");
		printf("  -r file - Read chip to file\n");
		printf("  -w file - Write chip from file\n");
		printf("  -v file - Verify chip from file data\n");
		printf("  -p <start pageno> <end pagenon>  - Select page to operate\n");
		printf("  -t reg  - Select region to read/write (main mem, oob ('spare') data or both, interleaved)\n");
		printf("  -s      - clock FTDI chip at 12MHz instead of 60MHz\n");
		printf("  -u vid:pid - use different FTDI USB vid/pid. Vid and pid are in hex.\n");
		exit(0);
	}


int main(int argc, char **argv) {
  int x, r;
	int vid=0, pid=0;
	int start_pageno = -1, end_pageno = -1;
	bool err=false;
	bool doSlow=false;
	string file="";
	enum Action {
		actionNone=0,
		actionID,
		actionRead,
		actionWrite,
		actionVerify
	};
  int f, opt;
  char *endptr;
  extern char *optarg;
  //char * file[256];
  char * buf[256];

	printf("FT2232H-based NAND reader");
	//Parse command line options
	Action action=actionNone;
	NandChip::AccessType access=NandChip::accessBoth;
  while ((opt = getopt(argc, argv, "iw:v:u:t:p:s:")) != -1) {		//: semicolon means that option need an arg!
    switch(opt) {
      case 'i':
			  action=actionID;
        break;
      case 'w':
        action=actionWrite;
        sprintf(file, "%s", optarg);
        break;
      case 'v':
        action=actionVerify;
        sprintf(file, "%s", optarg);
        break;
      case 'u':
        action=actionVerify;
        sprintf(buf, "%s", optarg);
			  //vid=strtol(argv[x], &endp, 16);
				//pid=strtol(endp, NULL, 16);
        break;
      case 't':
        action=actionVerify;
        sprintf(buf, "%s", optarg);
			  if (strcmp(buf,"main")==0) {
				  access=NandChip::accessMain;
			    } else if (strcmp(buf, "oob")==0) {
				        access=NandChip::accessOob;
			          } else if (strcmp(buf, "both")==0) {
				                access=NandChip::accessBoth;
			                } else {
				                printf("Must be 'main' 'oob' or 'both': %s\n", buf);
                      }
        break;
      case 'p':
        action=actionVerify;
        sprintf(buf, "%s", optarg);
        start_pageno = strtol(argv[x], NULL, 10);
        x++;
        end_pageno = strtol(argv[x], NULL, 10);
        break;
      case 's':
			  doSlow=true;
        break;
      default:
        usage();
        break;
      }
    }

	if (start_pageno == -1)
		start_pageno = 0;

	FtdiNand ftdi;
	ftdi.open(vid,pid,doSlow);
	NandChip nand(&ftdi);

	if (action==actionID) {
		nand.showInfo();
	} else if (action==actionRead || action==actionVerify) {
		int f;
		if (action==actionRead) {
			f=open(file.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
		} else {
			f=open(file.c_str(), O_RDONLY);
		}
		if (f<0) {
			perror(file.c_str());
			exit(1);
		}
		NandID *id=nand.getIdPtr();
		long pages=(id->getSizeMB()*1024LL*1024LL)/id->getPageSize();
		int size=0;
		if (access==NandChip::accessMain) size=id->getPageSize();
		if (access==NandChip::accessOob) size=id->getOobSize();
		if (access==NandChip::accessBoth) size=id->getOobSize()+id->getPageSize();
		char *pageBuf=new char[size];
		char *verifyBuf=new char[size];
		int verifyErrors=0;
		nand.showInfo();
		printf("%sing %li pages of %i bytes...\n", action==actionRead?"Read":"Verify", pages, id->getPageSize());

		if (end_pageno == -1)
		{
			end_pageno = pages;
		}

		for (x = start_pageno; x<end_pageno; x++) {
			nand.readPage(x, pageBuf, size, access);
			if (action==actionRead) {
				r=write(f, pageBuf, size);
				if (r!=size) {
					perror("writing data to file");
					exit(1);
				}
			} else {
				r=read(f, verifyBuf, size);
				if (r!=size) {
					perror("reading data from file");
					exit(1);
				}
				for (int y=0; y<size; y++) {
					if (verifyBuf[y]!=pageBuf[y]) {
						verifyErrors++;
						printf("Verify error: Page %i, byte %i: file 0x%02hhX flash 0x%02hhX\n", x, y, verifyBuf[y], pageBuf[y]);
					}
				}
			}
			if ((x&15)==0) {
				printf("%i/%li\n\033[A", x, pages);
			}
		}
		if (action==actionVerify) {
			printf("Verify: %i bytes differ between NAND and file.\n", verifyErrors);
		}
	} else {
	 int f;
	 f = open(file.c_str(), O_RDONLY);
	 if (f<0) {
		 perror(file.c_str());
		 exit(1);
	 }

	 NandID *id = nand.getIdPtr();
	 long pages = (id->getSizeMB() * 1024LL * 1024LL) / id->getPageSize();
	 int size = 0;
	 if (access == NandChip::accessMain) size = id->getPageSize();
	 if (access == NandChip::accessOob) size = id->getOobSize();
	 if (access == NandChip::accessBoth) size = id->getOobSize() + id->getPageSize();
	 char *pageBuf = new char[size];
	 nand.showInfo();
	 printf("Writinging %li pages of %i bytes...\n", pages, id->getPageSize());
	 
	 if (end_pageno == -1)
	 {
		 end_pageno = pages;
	 }

	 for (x = start_pageno; x<end_pageno; x++) {
		 r = read(f, pageBuf, size);
		 if (r != size) {
			 perror("reading data from file");
			 exit(1);
		 }
		 
		 if (x % 32 == 0)
		 {
			 nand.eraseBlock(x);
		 }

		 int err = nand.writePage(x, pageBuf, size, access);
		 if ((x & 15) == 0) {
			 printf("%i/%li\n\033[A", x, pages);
		 }
	 }
   }
	
	printf("All done.\n"); 
	return 0;
}

