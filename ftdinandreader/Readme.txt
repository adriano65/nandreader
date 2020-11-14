router model -> TG799vn VDNT-S
SN -> CP1316RAUGG
AK 69FMCJW7
Main SoC -> bcm63288DVKFEBG
BrcmNAND mfg 2c f1 MICRON MT29F1G08ABA 128MB on CS0

boot= 0 - 0x40000 (262144 dec = 256 kB)

--------------------Status OLD
CP1416RAUGG MAC 9c:97:26:d3:8b:1e -> onlybootp
CP1440RADH9 MAC 30:91:8f:49:ef:ba -> no decompressing
CP1504SAL7M MAC 30:91:8f:9c:7e:c4 -> reference!

--------------------Switch bank 1
La questione è molto semplice: c'è un pin del chip nand (il numero 8  del package TSOP 48) che si chiama RE# (read enable) e 
fa esattamente quello che dice di fare, se lo si mette basso (ndr: a terra, quindi collegato ad esempio alla parte metallica 
della porta usb della board che è bella grande e comoda anche per incastrarci dentro un bel cavetto affinché rimanga fermo lì 
senza rompere le scatole) disabilita la lettura. Subito a fianco del pin la board ha una serie di piazzole tonde che espongono i 
vari contatti, quindi per mirare il pin giusto vi consiglio di usare quelle.
Il trucco sta nel continuare ripetutamente a toccare e non toccare quel contatto col filo collegato a terra in modo da 
corrompere qualche lettura qui e li. Finito di caricare, il kernel risulta corrotto e la board fa reboot su attempt 2, senza scampo

Cosa importante il force switch bank è momentaneo...
Quindi una volta che  parte il bank 1 fate il root e caricate la gui di ansuel.
---------------------------------------------------------------------------------------------------------------------

Prima di ricaricare il firmware AGVTF_x.x.x devi seguire la procedura di reset che consiste nello 
spegnere il router premere il pulsante reset, accendere tenendo sempre premuto per 90 sec, rilasciare e 
aspettare la fine del boot (tutto senza connessioni a linea telefonica wifi c ecc). 
A router acceso premere reset e mantenere premuto reset per 60sec e lasciar riavviare da solo senza spegnere 
(così si pulisce completamente il factory "corrotto". Dopo che il router si è avviato del tutto, spegnere dal tasto off e seguire la procedura di caricamento.


---------------------------------------------------------------------------------------------------------------------
Decrypted RBI firmwares are the same as bank_1 or bank_2 dumps except for their first four bytes. 
A correctly decrypted RBI starts with a sequence of four 0xFF. 
You can edit these bytes to 0x00 and use the resulting file as a bank dump to be restored.

------------- bootpd --------------------------------------------------
dhcpd -f -d eth6

---------------------------------------------------------------------------------------------------------------------
dd if=1024AGVTF of=vant-f_CRF687-16.3.7567-660-RG-my.rbi bs=1 count=1024 conv=notrunc

binwalk -e inflated_AGVTF_1.0.0_009_closed.rbi

AGVTF_4.0.0_012_open.rbi
git clone https://github.com/pedro-n-rocha/bli223dcryptex.git
https://forums.whirlpool.net.au/forum-replies.cfm?t=2650998&p=15#r289
http://192.168.1.1/wanStatus.lp

------------- Check a public key --------------------------------------------------
openssl rsa -inform PEM -pubin -in AGpubkey.pem -text -noout
openssl pkey -inform PEM -pubin -in cert1_pubkey.pem -text -noout

------------- Obtain public key from certificate -------------------------------
openssl x509 -pubkey -noout -in cert4.pem

----------- Check a private key ---------------------------------------------------
openssl rsa -in cert4_privkey.pem -check

-------------- Converting from PKCS#8 to PKCS#1 can be done with 
The unencrypted PKCS#8 encoded data starts and ends with the tags 
-----BEGIN PRIVATE KEY-----
BASE64 ENCODED DATA
-----END PRIVATE KEY-----

openssl rsa -in cert4_privkey.pem -out cert4_privkey18.pem


-------------- Converting from PKCS#1 to PKCS#8 can be done with 
The unencrypted PKCS#1 encoded data starts and ends with the tags 
-----BEGIN RSA PRIVATE KEY-----
BASE64 ENCODED DATA
-----END RSA PRIVATE KEY-----

openssl pkey -in key1.pem -out key8.pem


---------- Check a cert vs private key by modulus ---------------------------------------------------
openssl x509 -in AGcert.cer -modulus -noout
openssl rsa -in AGprivkey.pem -modulus -noout

---------- Obtain public key from private key -------------------------------
openssl rsa -in cert3_privkey.pem -pubout
openssl rsa -in AGprivkey.pem -pubout -out public_key.pem

Obtain param of public key -------------------------------
openssl rsa -pubin -in AGpubkey.pem -modulus -text
echo "ibase=16; CCD40F322CA130B5E1797CFC9315ADF98B7322648A089A17C6610FC989D0B6908A12BBCA990D249A8B028B6FB662AD2110D5FE2737D8400B434E4059CFF047F7E21C9E01CDE9D6D3BCCA7BE2B93BF28AD8D34F1540E37CF2DC14FC976C3455359854302F967EB6BA99F669771D1C9BCF9ADAAEDAD2D8D2B962841D181B240825C4BF8A3F90ADBF8498402FBAE0CBD59853C0ED8B9EDCDF4989EBC7C0E7323DC128BAA5745EDBC5A6E80A92281F98202120385FD1077E794D0F78B219D90681C65C04BA69E9D8799BE3AC07200925C854ED49AD4EDE6ECB3F1B6A7F13A57E870F5B07B8A51F0945909E3BF346D3CA7531E981826F50ED87D17D2CDF8821C4D575"|bc


https://github.com/mswhirl/bouncer

Broadcom4350
bank 2 at address 0xc4500000
Broadcom BCM63268
-----------------------------------------------------------

I don't have a clear cut set of instructions to get ssh.

But it involves creating an EXT2 (no EXT3 ?) usb key.
Create a symlink folder called 'rootfolder' which points to '\' directory.

It's basicly a folder shortcut from the usb drive to the device root dir.

Once the device mounts and shares the key, you can browse to the keys' symlink folder. Like
\\192.168.1.1\UsbKey\rootfolder\etc
\\192.168.1.1\UsbKey\rootfolder\var
\\192.168.1.1\UsbKey\rootfolder\ dosn't work.

Once you can get that, you can upload a dropbear MIPS binary, and execute it by modifying the smb.conf to execute the commands on connection.

I did make a script to automate that part, as it clears on reboot. But gives you software access to the nand.

-----------------------------------------------------------

02 GND | 01 nTRST (3k pull-down) 
04 GND | 03 TDI (10k pull-up) 	orange
06 GND | 05 TDO				  	blue
08 GND | 07 TMS (10k pull-up) 	yellow
10 GND | 09 TCK (10k pull-up) 	green
	NC | 11 nSRST (10k pull-up)
14 3v3 | 13 N/C - or - DINT

------------------------------------------------------------decrypt
openssl aes-128-cbc -K a0dd1da4242d32424fdffaa0ed0e0f12 -nosalt -iv 0 -d -in TechnicolorAGConfig_2011-05-01T021056.cfg -out DECRYPTED-BACKUPFILE.xml

-------------------------------------------------------------------encrypt
openssl aes-128-cbc -K a0dd1da4242d32424fdffaa0ed0e0f12 -nosalt -iv 0 -e -in DECRYPTED-BACKUPFILE.xml -out Technicolor.cfg


-----------------------------------------------------------
nc -lvvp 5001
:::::::;nc 192.168.1.5 5001 -e /bin/sh


-----------------------------------------------------------
http://192.168.1.1/cgi/b/ST/?be=0&I0=0&I1=-1


-----------------------------------------------------------
Ctrl: TFTP started (Rx:technicolor tg789vn tg799vn R843H_prem.bli).
Checking fia's fia[0]=85, exp_fia[0]=80 fia[1]=84, exp_fia[1]=74


------------------------------------------------------------decrypt -- NO
openssl rsautl -decrypt -inkey test3.pem -in "technicolor tg789vn tg799vn R843H_prem.bli" -out "technicolor tg789vn tg799vn R843H_prem.bli.dec"
openssl rsautl -decrypt -inkey test3.pem -in "AGVTF_4.0.0_012_open.rbi" -out "AGVTF_4.0.0_012_open.rbi.dec"


------------------------------------------------------------FW correctly loaded via bootp/tftp by bootloader
********* fw fixed header decoding ********************************************************************* 

 [               ] filename          : ../AGVTF_5.3.2_003_closed.rbi
 [ptr : 000      ] imagetype         : BLI223PJ0
 [ptr : 006      ] FIACODE           : PJ
 [ptr : 020      ] branding          : 0
 [ptr : 028      ] flag??            : 00000000
 [ptr : 032      ] version           : 10.5.35.W
 [ptr : 040      ] hdrlength         : 0x00000163
 [ptr : 044      ] datalength        : 0x00cf97af
 [ptr : 048      ] crc32             : 2677a8e6
 [ptr :          ] computedhash      : b305aa45eac6d5549c187fe07cf140968a3d42e6
 [ptr :          ] decryptedhash     : b305aa45eac6d5549c187fe07cf140968a3d42e6
 [ptr :          ] integritycheck    : True
 [ptr :          ] aeskey            : e0007218592ac40d9ff7428a48504ea85ee96f430493af368c65c8067061feb0
 [prt : 0xcf9912 ] footer            :  
 [ptr : 356      ] imagetype         : MUTE
 [ptr : 310      ] board             : VDNT-S 
 [ptr : 318      ] model1            : Technicolor TG799 
 [ptr : 337      ] model2            : TG799n 
 [ptr : 345      ] flashaddr         : c2000000 
 [ptr : 351      ]  undefined : 1ed9da87 

******************************************************************************************************** 

------------------------------------------------------------FW tag analyzer -- NOT working
./analyzetag -i flashdump.bin -s c2000000 -t ag306



----------------------------------------------------------------------------------- boot
Found NAND on CS0: ACC=f7ff1010, cfg=15142200, flashId=2cf18095, tim1=6574845b,
 tim2=00001e96
BrcmNAND version = 0x0400 128MB @00000000
brcmnand_scan: Done brcmnand_probe
brcmnand_scan: B4 nand_select = 80000000
brcmnand_scan: After nand_select = 80000000
100 CS=0, chip->ctrl->CS[0]=0
ECC level 15, threshold at 1 bits
reqEccLevel=0, eccLevel=15
190 eccLevel=15, chip->ecclevel=15, acc=f7ff1010
brcmnand_scan 10
200 CS=0, chip->ctrl->CS[0]=0
200 chip->ecclevel=15, acc=f7ff1010
page_shift=11, bbt_erase_shift=17, chip_shift=27, phys_erase_shift=17
brcmnand_scan 220
Brcm NAND controller version = 4.0 NAND flash size 128MB @18000000
brcmnand_scan 230
brcmnand_scan 40, mtd->oobsize=64, chip->ecclayout=00000000
brcmnand_scan 42, mtd->oobsize=64, chip->ecclevel=15, isMLC=0, chip->cellinfo=0
ECC layout=brcmnand_oob_bch4_4k
brcmnand_scan:  mtd->oobsize=64
brcmnand_scan: oobavail=50, eccsize=512, writesize=2048
brcmnand_scan, eccsize=512, writesize=2048, eccsteps=4, ecclevel=15, eccbytes=3
300 CS=0, chip->ctrl->CS[0]=0
500 chip=8d964980, CS=0, chip->ctrl->CS[0]=0
brcmnand_scan 99
Gateway flash mapping
[NAND] : tBBT loaded
----------------------------------------------------------------------------------- boot
to load first 128 kB (0x20000) 
1. erase first block
./nandread_FTDI232H -p 0 -e 1


2. load 64 block (of 2kB each)
./nandread_FTDI232H -f CP1416RAUGG-onlybootp.bin -s 0 -w 64

3. check
./nandread_FTDI232H -f bootloader.bin -s 0 -r 64

----------------------------------------------------------------------------------- read bank 1 (partially)
./nandread_FTDI232H -f bank1 -s 16384 -r 256
---Erase bank1 0x2000000-0x4500000
./nandread_FTDI232H -p 256 -e 296
./nandread_FTDI232H -f kernel_test.bin -s 16384 -w 256

----------------------------------------------------------------------------------- read bank 2
./nandread_FTDI232H -f bank2 -s 35328 -r 18944
 

----------------------------------------------------------------------------------- 
binwalk --signature --term inflated_AGVTF_4.0.0_012_open.rbi
dd if=inflated_AGVTF_4.0.0_012_open.rbi of=boh.lzma bs=1 skip=32 count=2097152
lzma: boh.lzma: Compressed data is corrupt

bank2 start 35328 total 18944  -> ends at 54272


 
----------------------------------------------------------------------------------- give this command many times to avoid nand writings error from ft2232HL adapter :-)
./ftdinandreader -w CP1504SAL7M-bootloader-withOOB.bin -p 0x00 0x81000 -t both

./ftdinandreader -r bootloader.bin -p 0x00 0x81000 -t main
 
----------------------------------------------------------------------------------- copy bank1 to bank 2
./ftdinandreader -r bank_1 -p 0x2000000 0x4500000 -t both
./ftdinandreader -w bank_1 -p 0x04500000 0x06a00000 -t both

AGVTF_1.0.0_008.rbi -> Linux version 2.6.30 (gcc version 3.4.6) #1 Sat Apr 28 17:41:44 CST 2012				--> no zlib in kernel!
AGVTF_2.0.0_010_closed.rbi -> Linux version 2.6.30 (gcc version 3.4.6) #1 Fri Jul 13 14:18:15 CST 2012		--> no zlib in kernel!
AGVTF_4.0.0_012_open.rbi -> Linux version 2.6.30 (gcc version 3.4.6) #1 Thu May 26 17:20:12 CEST 2016		-- kernel boot but jffs2 read only

jffs2_scan_eraseblock(): Magic bitmask 0x1985 not found at 0x00f80000: 0x0008 instead
./ftdinandreader -r userfs -p 0x80000 0x2000000 -t main

test --------------------------- OK erased userfs to fix jffs2
./ftdinandreader -e -p 0x80000 0x2000000

-------------------------------------------------- ?
qlzma_un: ZLIB decompression is not supported                                                       
------------[ cut here ]------------                                                                 
WARNING: at fs/squashfs/lzma/uncomp.c:141 0x800d71e4()                                               
Modules linked in:                                                                                   
                                                                                                     
Call Trace:(--Raw--[<80036c10>] 0x80036c10                                                           
[<80016cc8>] 0x80016cc8                                                                              
[<80016cc8>] 0x80016cc8                                                                              
[<80035ab8>] 0x80035ab8                                                                              
[<800d71e4>] 0x800d71e4                                                                              
[<800d71e4>] 0x800d71e4                                                                              
[<80064538>] 0x80064538                                                                              
[<800d2a80>] 0x800d2a80                                                                              
[<800d2af8>] 0x800d2af8                                                                              
[<800d4a70>] 0x800d4a70  


----------------------------------------------------------------- AGVTF_5.2.1_closed.rbi
<5>Technicolor nand flash translation layer initialized.                                             
flash mapping initialized                                                                            
parse_btab: num_banks (5)                                                                            
bank #0: 0, 80000 (<NULL>)                                                                           
bank #1: 0, 2000000 (<NULL>)                                                                         
bank #2: 0, 4500000 (<NULL>)                                                                         
bank #3: 0, 20000 (<NULL>)                                                                           
bank #4: 0, 40000 (<NULL>)                                                                           
Creating 1 MTD partitions on "technicolor-nand-tl":                                                  
0x000002200000-0x000004d00000 : "rootfs"      --> squash                                                       
Creating 5 MTD partitions on "technicolor-nand-tl":                                                  
0x000000080000-0x000002000000 : "userfs"                                                             
0x000002000000-0x000004500000 : "bank_1"                                                             
0x000004500000-0x000006a00000 : "bank_2"                                                             
0x000000020000-0x000000040000 : "eripv2"                                                             
0x000000040000-0x000000080000 : "rawstorage"

0x000006a00000-0x000008000000 : "kernels bank..." 
0x000002000000-0x000002200000 kernel 1 ?
-------------------------------------------------------------> no ! userfs ends @ 0x4d00000 .... so overwrites bank_2: limit to 0x4500000!!
./ftdinandreader -r rootfs -p 0x2200000 0x4500000 -t both

dd if=rootfs.jffs2 of=rootfs.squash bs=1 skip=1115136 count=12711002

sasquatch -s rootfs.squash
--can't extract

----------------COLLAGE ------------------------------ :-)
mksquashfs _inflated_AGVTF_5.2.1_closed.rbi.extracted rootfs.squash
cp rootfs.squash rootfs.new
dd if=rootfs of=firstpart bs=1 count=1115136
cat rootfs.new >> firstpart
mv firstpart rootfs.new

./ftdinandreader -e -p 0x2310400 0x4500000
./ftdinandreader -w rootfs.new -p 0x2310400 0x4500000 -t main


----------------------------------------------------------------> no ! userfs ends @ 0x4d00000 .... so overwrites bank_2!!
./ftdinandreader -w rootfs.new -p 0x2200000 0x4500000 -t main

----------------------------------------------------------------------------------- write bank1
./ftdinandreader -r CP1504SAL7M-bank1-withOOB.bin -p 0x2000000 0x4500000 -t both
./ftdinandreader -w CP1504SAL7M-bank1-withOOB.bin -p 0x2000000 0x2200000

----------------------------------------------------------------------------------- copy bank1 to bank 2 again
./ftdinandreader -r bank_1 -p 0x2000000 0x2200000 -t both
./ftdinandreader -w CP1504SAL7M-bank1-withOOB.bin -p 0x4500000 0x06a00000 -t both

128 kiB == 0x20000
0x4500000 / 0x20000 = 228 hex == 552 dec

./ftdinandreader -w bank_1 -p 0x04500000 0x04700000 -t both
./ftdinandreader -r bank_1-noOOB.bin -p 0x2000000 0x4500000 -t main
./ftdinandreader -r userfs-noOOB.bin -p 0x80000 0x2000000 -t main

./ftdinandreader -w bank_1-noOOB-new.bin -p 0x4500000 0x6a00000 -t main

./ftdinandreader -w bank_1-noOOB-new.bin -p 0x2000000 0x4500000 -t main
./ftdinandreader -w bank_1-noOOB-new.bin -p 0x2000000 0x22A22F0 -t main	 kernel :)

./ftdinandreader -w CP1504SAL7M-bank1-withOOB.bin -p 0x2000000 0x2300000 -t both
./ftdinandreader -r bank_1-OOB.bin -p 0x2000000 0x2300000 -t both


./ftdinandreader -w CFE.BIN -p 0x00 0x20000 -t main			--> NO

----------------------------------------------------------------------------------- erase verify
./ftdinandreader -e -p 0x4500000 0x4700000
./ftdinandreader -r bank_2 -p 0x4500000 0x4700000 -t main

./ftdinandreader -w CP1504SAL7M-bootloader-withOOB.bin -p 0x6a00000 0x6c00000 -t main

./ftdinandreader -w bank_1-noOOB.bin -p 0x2000000 0x22F0000 -t both --> try to load kernel only


----------------------------------------------------------------------------------- try injecting only squashfs
name@cxlabs:~/nandreader/ftdinandreader$ binwalk bank_1-noOOB.bin 

DECIMAL       HEXADECIMAL     DESCRIPTION
--------------------------------------------------------------------------------
26            0x1A            LZMA compressed data, properties: 0x5D, dictionary size: 2097152 bytes, uncompressed size: 2761456 bytes
2097152       0x200000        Squashfs filesystem, little endian, non-standard signature, version 4.0, compression:gzip, size: 12723158 bytes, 1354 inodes, blocksize: 65536 bytes, created: 2016-05-26 16:02:13

0x2000000-0x000004500000 : "bank_1"                                                             
0x200000

./ftdinandreader -w _bank_1-noOOB.bin.my/200000.my -p 0x2200000 0x2D8D000 -t main


----------------------------------------------------------------------------------- extracting kernel
./ftdinandreader -r kernel-withOOB2.bin -p 0x2000000 0x22A22F0
./ftdinandreader -w kernel-OOB.bin -p 0x4500000 0x47A22F0

which ECC algorithm is used by the NAND sub-system. By default, both u-boot and Linux use BCH8


./ftdinandreader -e -p 0x44F7000 0x04539000
./ftdinandreader -r bank_2 -p 0x44F7000 0x04539000


-----------------------------------------------------------------------------------
./ftdinandreader -e -p 0x80000 0x2000000
./ftdinandreader -w userfs-noOOB.bin -p 0x80000 0x2000000 -t oob

----------------------------------------------------------------------------------- load via tftp/bootp
./ftdinandreader -e -p 0x40000 0x6A00000
load: wrong nand ecc error goes away, but no kernel boot

./ftdinandreader -e -p 0x40000 0x8000000

extract eripv2 
dd if=bootloader-withOOB.bin of=eripv2-withOOB.bin bs=1 count=135168 skip=135168
./ftdinandreader -w eripv2-withOOB.bin -p 0x20000 0x40000

-- FIX eripv2 --> NO
./ftdinandreader -e -p 0x20000 0x80000
./ftdinandreader -w bootloader/eripv2-withOOB.bin -p 0x20000 0x40000
./ftdinandreader -w bootloader/rawstorage-withOOB.bin -p 0x40000 0x80000

-- FIX eripv2 --> NO
./ftdinandreader -e -p 0x0 0x80000
./ftdinandreader -w bootloader/bootloader-withOOB.bin -p 0x00 0x80000

-- FIX eripv2 --> NO
./ftdinandreader -e -p 0x40000 0x6A00000



------------------------------------------------------------------------------------ this seems to correct better reboot loop
./ftdinandreader -w bootloader/rawstorage-withOOB.bin -p 0x40000 0x80000 -t skip
YES!! firmware bootp loaded and linux comes up

---- NO
./ftdinandreader -e -p 0x2200000 0x2DE0000
./ftdinandreader -w _bank_1-noOOB.bin.my/200000.my -p 0x2200000 0x2D8D000 -t main
---- NO


------------------------------------------------------------------------------------ OK correct reboot loop and RIP2
./ftdinandreader -e -p 0x00 0x8000000
./ftdinandreader -w bootloader/bootloader-withOOB.bin -p 0x00 0x80000


--------------------------------------------------------------------------------------- Extract kernel
./ftdinandreader -r kernel-0x2060000.bin -p 0x2060000 0x2300000 -t main



restore old kernel
./ftdinandreader -e -p 0x2000000 0x2200000
./ftdinandreader -w kernel/kernel-20000000-withOOB.bin -p 0x2000000 0x2200000 --> was extracted from 2000000
==> NO

WHERE IS KERNEL ???
./ftdinandreader -r test -p 0x100000 0xF00000 -t main

./ftdinandreader -r test2 -p 0x22F000 0x230000 -t main


------------------------------------------------------------ change Rescue user uid/gid to 0
./ftdinandreader -r test2 -p 0x220000 0x240000 -t main
edit jffs2 block
./ftdinandreader -e -p 0x220000 0x240000
./ftdinandreader -w test2 -p 0x220000 0x240000 -t oob
at boot brcmnand reconize bad blocks at 220000 and mark them as bad 


------------------------------------------------------------ try load 4.0 FW
no remove rawstorage partition
./ftdinandreader -e -p 0x80000 0x6A00000


WHERE IS KERNEL ???
./ftdinandreader -r test2 -p 0x6a00000 0x8000000 -t main

---------------------------------------------------------------------- Test 1 - falso allarme
6a00000+13E0000 = 7DE0000 --> ?
./ftdinandreader -r test -p 0x7DE0000 0x7E80000 -t main  --> nothing


6a00000+1480000 = 7E80000 --> Kernel ! bank 2 ?
./ftdinandreader -r test -p 0x7E80000 0x7F00000 -t main  --> nothing
now check where it finish
0x8000000-0x7E00000 --> MAX 0x180000 ! 

./ftdinandreader -e -p 0x7E80000 0x8000000
./ftdinandreader -w kernel/kernel_my.bin -p 0x7E80000 0x8000000 -t oob
./ftdinandreader -w kernel/kernel_my.bin -p 0x7E80000 0x8000000 -t main


-------------------------------------------- rootfs, squash fs
SQUASHFS error: Can't find a SQUASHFS superblock on mtdblock1
./ftdinandreader -e -p 0x2200000 0x4D00000
./ftdinandreader -w _bank_1-noOOB.bin.my/200000.my2 -p 0x2200000 0x2300000 -t oob -----> NO

./ftdinandreader -e -p 0x2000000 0x2100000
./ftdinandreader -w _bank_1-noOOB.bin.my/200000.my -p 0x2000000 0x2100000 -t oob ---------> NO + kernel NOT LOADED

-----------------------  restore 0x2000000 0x2200040  
./ftdinandreader -e -p 0x2000000 0x2200040
./ftdinandreader -w CP1504SAL7M/CP1504SAL7M-bank1-withOOB.bin -p 0x2000000 0x2200040  --> NO !

---Reload FW
./ftdinandreader -e -p 0x80000 0x8000000

---------------------------------------------------------------------- kernel Test2 is here now !?
./ftdinandreader -r test -p 0x2020000 0x2100000 -t main
./ftdinandreader -e -p 0x2020000 0x2100000
./ftdinandreader -w kernel/kernel_my.bin -p 0x2020000 0x2100000 -t add


--------------------------------------------- some mips elf binaries discovered
./ftdinandreader -r test -p 0x7f80000 0x8000000 -t main

./ftdinandreader -r test -p 0x2220000 0x2300000 -t main --> squash

--------------------------------------------- ecc stuff.. OK!!
./createnfimg -j 0 -b 1 -i test

./createnfimg -j 0 -b 1 -i kernel_my.bin

./ftdinandreader -e -p 0x2000000 0x2200000
./ftdinandreader -w kernel/kernel_my.out -p 0x2000000 0x2200000
---> NO: reload old FW
./ftdinandreader -w kernel/kernel-2000000.2200000-wOOB.bin -p 0x2000000 0x2200000


--------------------------------------------- rootfs
./ftdinandreader -r test -p 0x2200000 0x2300000

./createnfimg -j 0 -b 1 -i rootfs.my

./ftdinandreader -e -p 0x2200000 0x4500000
./ftdinandreader -w rootfs.out -p 0x2200000 0x4500000
