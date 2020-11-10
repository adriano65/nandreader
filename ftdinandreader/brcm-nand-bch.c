#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bch.h"

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
static void shift_half_byte(const uint8_t *src, uint8_t *dest, size_t sz){
	// go right to left since input and output may overlap
	unsigned j;
	dest[sz] = src[sz - 1] << 4;
	for (j = sz; j != 0; --j)
		dest[j] = src[j] >> 4 | src[j - 1] << 4;
	dest[0] |= src[0] >> 4;
}

int brcm_nand(unsigned poly, unsigned char *data) {
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
