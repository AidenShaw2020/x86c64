/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <conio.h>
#include <stdio.h>
#include "../src/c64/drive1581.h"
uint8_t check_pass,check_fail;
uint32_t check_signature;
static uint8_t data[512];
static const uint16_t sectors[]={779,780,1439};
int main(void) {
    uint16_t i,lba; uint8_t n;
    clrscr(); puts("FD64 / 1581 RAW SECTOR TEST");
    if(fd_1581_open()) { check_fail=1; goto done; }
    if(fd_1581_read(0,data) || data[510]!=0x55 || data[511]!=0xaa) { check_fail=2; goto done; }
    if(fd_1581_read(1440,data)!=-1 || fd_1581_write(1440,data)!=-1 ||
        fd_1581_reads!=1 || fd_1581_writes!=0) { check_fail=3; goto done; }
    for(n=0;n<3;++n) {
        lba=sectors[n];
        for(i=0;i<512;++i) data[i]=(uint8_t)(i^lba^0xa5);
        if(fd_1581_write(lba,data)) { check_fail=4; goto done; }
    }
    /* Close/reopen so success cannot come from our C64 memory buffer. */
    fd_1581_close();
    if(fd_1581_read(0,data)!=-1 || fd_1581_write(0,data)!=-1 || fd_1581_open()) { check_fail=5; goto done; }
    for(n=0;n<3;++n) {
        lba=sectors[n];
        if(fd_1581_read(lba,data)) { check_fail=6; goto done; }
        for(i=0;i<512;++i) if(data[i]!=(uint8_t)(i^lba^0xa5)) { check_fail=7; goto done; }
    }
    check_pass=1; puts("1581 READ / WRITE / REOPEN PASS");
done:
    fd_1581_close();
    if(check_fail) printf("FAIL %u DOS %u\n",check_fail,fd_1581_error);
    check_signature=0x31353831UL;
    for(;;) {}
    return 0;
}
