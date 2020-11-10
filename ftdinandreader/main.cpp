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
#include "stdlib.h"
#include "string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "FtdiNand.hpp"
#include "FtdiNand_BB.hpp"
#include "NandChip.hpp"
#include "NandCmds.h"

//Windows needs O_BINARY to open binary files, but the flag is undefined
//on Unix. Hack around that. (Thanks, Shawn Hoffman)
#ifndef O_BINARY
#define O_BINARY 0
#endif

int setParam(NandID *id, long *start_address, long *end_address, int *start_pageno, int *end_pageno, int *size );
int usage();

int main(int argc, char **argv) {
  FILE *fp;
  bool doSlow=true;
  NandChip *pNandChip;

  NandChip::AccessType accessType=NandChip::PageplusOOB;
  unsigned long start_address, end_address, address, fileaddress=0;
	int x, r, lenght, vid=0, pid=0;
	string file="", fileOut="";
  int verifyErrors=100;
  unsigned char *tmpBuf;

  //nand = new NandChip();
	printf("FT2232H-based NAND reader\n");
	NandChip::Action action=NandChip::actionNone;
	for (x=1; x<argc; x++) {
		if (strcmp(argv[x],"-i")==0) { action=NandChip::getID;}
    else if (strcmp(argv[x],"-e")==0 && x<=(argc-2)) { action=NandChip::actionErase; }
    else if (strcmp(argv[x],"-r")==0 && x<=(argc-2)) { action=NandChip::actionRead;	file=argv[++x]; }
    else if (strcmp(argv[x],"-w")==0 && x<=(argc-2)) { action=NandChip::actionWrite; file=argv[++x]; }
    else if (strcmp(argv[x],"-v")==0 && x<=(argc-2)) { action=NandChip::actionVerify;	file=argv[++x];	}
    else if (strcmp(argv[x],"-u")==0 && x<=(argc-2)) { action=NandChip::actionVerify;
			char *endp;
			x++;
			vid=strtol(argv[x], &endp, 16);
			if (*endp!=':') {
        action=NandChip::actionNone;
			  }
      else {
				endp++;
				pid=strtol(endp, NULL, 16);
			  }
		  }
    else if (strcmp(argv[x],"-t")==0 && x<=(argc-2)) {
			x++;
			if (strcmp(argv[x],"main")==0) {
				accessType=NandChip::Page;
			} else if (strcmp(argv[x], "mainoob")==0) {
				accessType=NandChip::PageplusOOB;
			} else if (strcmp(argv[x], "oob")==0) {
				accessType=NandChip::recalcOOB;
			} else if (strcmp(argv[x], "add")==0) {
				accessType=NandChip::addOOB;
			} else if (strcmp(argv[x], "skip")==0) {
				accessType=NandChip::skipOOB;
			} else if (strcmp(argv[x], "bb")==0) {
				accessType=NandChip::useBitBang;
			} else {
				printf("Must be 'main', 'oob' (recalc OOB), skip (skip OOB), add (add OOB) or bb (Bitbang mode)%s\n", argv[x]);
			}
		  }
    else if (strcmp(argv[x],"-f")==0) { doSlow=false;	} 
    else if (strcmp(argv[x], "-p")==0 && x <= (argc - 3)) {
                        x++;
                        start_address = strtol(argv[x], NULL, 16);
                        x++;
                        end_address = strtol(argv[x], NULL, 16);
                }    
    else {
			printf("Unknown option or missing argument: %s\n", argv[x]);
      action=NandChip::actionNone;
		  }
	  }
  if (action==NandChip::actionNone) { usage(); exit(0);}

  pNandChip=new NandChip(vid, pid, doSlow, accessType, action, start_address, end_address);

  switch (action) {
    case NandChip::getID:
      break;

    case NandChip::actionRead:
      if (pNandChip->checkAddresses(action)==0) {
        if ((fp=fopen(file.c_str(), "wb+")) == NULL) { perror(file.c_str()); exit(1); }
        printf("Reading from 0x%08X to 0x%08X (%s)\n", pNandChip->start_address, pNandChip->end_address, pNandChip->accessType==NandChip::Page ? "Page" : "Page+OOB" );
        for (address=pNandChip->start_address; address<pNandChip->end_address; address+=pNandChip->nandPageSize) {
          lenght=pNandChip->readPage(address);
          r=fwrite(pNandChip->pageBuf, 1, lenght, fp);
          if (r!=lenght) { printf("Error writing data to file\n"); break; }
          //printf("0x%08X / 0x%08X\n", address, (pNandChip->start_address-pNandChip->end_address)*pNandChip->pageSize);
          if ((address&0xF)==0) {
            printf("0x%08X / 0x%08X\n\033[A", address, pNandChip->end_address);
            }
          }
        fclose(fp);
        }
      break;
    
    case NandChip::actionVerify:
      tmpBuf=new unsigned char[pNandChip->nandPageSize];
      if (pNandChip->checkAddresses(action)) {
        if ((fp=fopen(file.c_str(), "rb")) == NULL) { perror(file.c_str()); exit(1); }
        printf("Verifying 0x%08X bytes of 0x%04X bytes (%s)\n", pNandChip->end_address-pNandChip->start_address, pNandChip->nandPageSize, pNandChip->accessType==NandChip::Page ? "Page" : "Page+OOB" );
        unsigned long address=pNandChip->start_address;
        while (!feof(fp)) {
          r=fread(tmpBuf, 1, pNandChip->filePageSize, fp);  //file is without OOB
          if (r!=pNandChip->filePageSize) { perror("reading data from file"); exit(1); }
          lenght=pNandChip->readPage(address);
          for (int y=0; y<pNandChip->nandPageSize; y++) {
            if (tmpBuf[y]!=pNandChip->pageBuf[y]) { printf("OOB calculated -->: address 0x%08X, byte %i: file 0x%02X flash 0x%02X\n", address+y, y, tmpBuf[y], pNandChip->pageBuf[y]); }
            }
          address+=pNandChip->nandPageSize;
          }
        fclose(fp);
        }
      delete[] tmpBuf;
      break;

    case NandChip::actionWrite:
      tmpBuf=new unsigned char[pNandChip->filePageSize];
      //verifyErrors=100;
      if (pNandChip->checkAddresses(action)==0) {
        if ((fp=fopen(file.c_str(), "rb")) == NULL) { perror(file.c_str()); exit(1); }
        printf("Writing from 0x%08X to 0x%08X (%s)\n", pNandChip->start_address, pNandChip->end_address, pNandChip->accessType==NandChip::Page ? "Page" : "Page+OOB" );        
        for (address=pNandChip->start_address; address<pNandChip->end_address; address+=pNandChip->nandPageSize) {
          r = fread(pNandChip->pageBuf, 1, pNandChip->filePageSize, fp);
          if (r != pNandChip->filePageSize) { 
            printf("\nInsufficient data from file: read 0x%04X, filePageSize 0x%04X\n", r, pNandChip->filePageSize);
            break;
            }
          printf("Writing 0x%08X / 0x%08X - FileAddress from 0x%08X to 0x%08X\n", address, pNandChip->end_address, fileaddress, fileaddress+pNandChip->filePageSize);
          fileaddress+=pNandChip->filePageSize;

          // add skip if buffer is 0xFF ..
          memset(tmpBuf, 0xFF, pNandChip->filePageSize);
          if (memcmp(tmpBuf, pNandChip->pageBuf, pNandChip->filePageSize)==0) continue;   // all FF

          //if (address % pNandChip->erasepageSize == 0) {
          //  if ( ! pNandChip->erasePage(address/pNandChip->erasepageSize) ) { printf("Erasing page %i FAILS", address); }
          //  }
          while (verifyErrors) {
            int err = pNandChip->writePage(address);
            memcpy(tmpBuf, pNandChip->pageBuf, pNandChip->filePageSize);
            lenght=pNandChip->readPage(address);
            //unsigned int size2verify;
            for (r=0; r<((pNandChip->accessType==NandChip::recalcOOB || pNandChip->accessType==NandChip::skipOOB) ? pNandChip->nandPageSize : pNandChip->filePageSize); r++) {
              if (tmpBuf[r]!=pNandChip->pageBuf[r]) { 
                printf("diff: address 0x%08X, byte %i: file 0x%02X flash 0x%02X\n", address+r, r, tmpBuf[r], pNandChip->pageBuf[r]); 
                verifyErrors--;
                break;
                }
              }
            if (r==((pNandChip->accessType==NandChip::recalcOOB) ? pNandChip->nandPageSize : pNandChip->filePageSize)) {
              break;
              }
            }
          if (!verifyErrors) { 
            printf("Too many errors\n"); 
            break; 
            }

          }
        fclose(fp);
        }
      break;

    case NandChip::actionErase:   // 131072 == 128 kiB
      if (pNandChip->checkAddresses(action)==0) {
        unsigned int erasepage;
        printf("Erasing %u pages of %i bytes\n", pNandChip->end_erasepageno-pNandChip->start_erasepageno, pNandChip->nandPageSize);
        for (erasepage=pNandChip->start_erasepageno; erasepage<pNandChip->end_erasepageno; erasepage++) {
          if (pNandChip->erasePage(erasepage)) { printf("page %u (0x%02X) (Address 0x%08X) erased\n", erasepage, erasepage, erasepage*pNandChip->erasepageSize); }
          else { printf("address from 0x%02X (0x%08X) NOT erased\n", erasepage, erasepage*pNandChip->erasepageSize); }
          }
        }
      break;

    default:
      printf("Boh???\n");
      break;
    }
  delete pNandChip;
	printf("\nAll done.\n");
}

int usage(){
  printf("Usage: %s [-i|-r file|-v file] [-t main|oob|both] [-s]\n");
  printf("  -i      - Identify chip\n");
  printf("  -r file - Read chip to file\n");
  printf("  -p 0x<start address> 0x<end address>  - Select address to operate\n");    
  printf("  -w file - Write chip from file (set addresses!!)\n");
  printf("  -v file - Verify chip from file data\n");
  printf("  -e      - Erase chip (set addresses!!)\n");
  printf("  -t reg  - Select region to read/write (main, oob recalculate or bb (Bitbang mode))\n");
  printf("  -f      - clock FTDI chip at 60MHz instead of 12MHz\n");
  printf("  -u vid:pid - use different FTDI USB vid/pid. Vid and pid are in hex.\n");
  exit(0);
}