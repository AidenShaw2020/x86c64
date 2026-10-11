/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pager.h"
#include "x86.h"
#include "demo.h"
#include "disk.h"
#include "bios.h"
static FILE *ram, *disk;
static int fail_read=-1, fail_write=-1, checks;
static long fail_sector=-1;
static char output[128];
static unsigned output_pos;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static int read_page(uint16_t page,uint8_t *data) {
    if(page==fail_read || page>=FD_RAM_PAGES) return -1;
    if(fseek(ram,(long)page*512,SEEK_SET)) return -1;
    return fread(data,1,512,ram)==512 ? 0:-1;
}
static int write_page(uint16_t page,const uint8_t *data) {
    if(page==fail_write || page>=FD_RAM_PAGES) return -1;
    if(fseek(ram,(long)page*512,SEEK_SET)) return -1;
    return fwrite(data,1,512,ram)==512 ? 0:-1;
}
static int read_sector(uint32_t lba,uint8_t *data) {
    if((long)lba==fail_sector) return -1;
    if(fseek(disk,(long)lba*512,SEEK_SET)) return -1;
    return fread(data,1,512,disk)==512 ? 0:-1;
}
static int write_sector(uint32_t lba,const uint8_t *data) {
    if(fseek(disk,(long)lba*512,SEEK_SET)) return -1;
    return fwrite(data,1,512,disk)==512 ? 0:-1;
}
static int bios(FDCPU *c,uint8_t n) {
    if(n!=0x10 || (c->r[AX]>>8)!=0x0e || output_pos>=sizeof(output)-1) return -1;
    output[output_pos++]=(char)c->r[AX]; output[output_pos]=0; return 0;
}
static int console_put(uint8_t ch) {
    if(output_pos>=sizeof(output)-1) return -1;
    output[output_pos++]=(char)ch; output[output_pos]=0; return 0;
}
static FDCPU execute(const uint8_t *bytes,unsigned length) {
    FDCPU c; unsigned i;
    for(i=0;i<length;++i) CHECK(fd_mem_write(0x7c00UL+i,bytes[i])==0);
    fd_cpu_init(&c); fd_cpu_run(&c,4096,bios); return c;
}
static void test_pager(void) {
    unsigned i; uint8_t v, page[512];
    fd_pager_init(read_page,write_page);
    for(i=0;i<32;++i) CHECK(fd_mem_write((uint32_t)i*512+511,(uint8_t)(i+1))==0);
    for(i=0;i<32;++i) { CHECK(fd_mem_read((uint32_t)i*512+511,&v)==0); CHECK(v==i+1); }
    CHECK(fd_mem_write(0xfffffUL,0x41)==0);
    CHECK(fd_mem_write(0x100000UL,0x42)==0);
    CHECK(fd_mem_read(0,&v)==0 && v==0x42);
    CHECK(fd_mem_read(0xfffffUL,&v)==0 && v==0x41);
    CHECK(fd_pager_flush()==0); CHECK(fflush(ram)==0);
    CHECK(read_page(2047,page)==0 && page[511]==0x41);
    CHECK(read_page(0,page)==0 && page[0]==0x42);
    fd_pager_init(read_page,write_page);
    for(i=0;i<8;++i) CHECK(fd_mem_write((uint32_t)i*512,0xa5)==0);
    fail_write=0;
    CHECK(fd_mem_read(8*512,&v)!=0); /* dirty eviction must fail */
    CHECK(fd_mem_read(0,&v)==0 && v==0xa5); /* dirty contents survive */
    CHECK(fd_pager_flush()!=0);
    fail_write=-1; fail_read=8;
    CHECK(fd_mem_read(8*512,&v)!=0); /* failed read preserves cache */
    CHECK(fd_mem_read(0,&v)==0 && v==0xa5);
    fail_read=-1; CHECK(fd_pager_flush()==0);
}
static void test_cpu(void) {
    FDCPU c; unsigned i;
    static const uint8_t adc[]={0xb0,0,0xf9,0x14,0xff,0xf4};
    static const uint8_t sbb[]={0xb0,0,0xf9,0x1c,0xff,0xf4};
    static const uint8_t inc[]={0xb8,0xff,0xff,0xf9,0x40,0xf4};
    static const uint8_t cross[]={0xc7,0x06,0xff,0x01,0xef,0xbe,0xa1,0xff,0x01,0xf4};
    static const uint8_t call[]={0xb8,1,0,0xe8,1,0,0xf4,0x40,0xc3};
    static const uint8_t illegal[]={0x66,0xf4};
    static const uint8_t loop[]={0xeb,0xfe};
    static const uint8_t unsupported_bios[]={0xcd,0x21};
    static const uint8_t bpcode[]={0x8b,0x46,0,0xf4};
    CHECK(fd_physical(0xffff,0x10)==0);
    CHECK(fd_demo_load()==0); output_pos=0; output[0]=0;
    fd_cpu_init(&c); CHECK(fd_cpu_run(&c,4096,bios)==FD_X86_HALT);
    CHECK(strcmp(output,fd_demo_expected)==0); CHECK(c.instructions>40);
    c=execute(adc,sizeof adc); CHECK(c.status==FD_X86_HALT);
    CHECK((c.r[AX]&255)==0 && (c.flags&(FD_CF|FD_ZF))==(FD_CF|FD_ZF));
    c=execute(sbb,sizeof sbb); CHECK((c.r[AX]&255)==0 && (c.flags&FD_CF));
    c=execute(inc,sizeof inc); CHECK(c.r[AX]==0 && (c.flags&FD_CF) && (c.flags&FD_ZF));
    c=execute(cross,sizeof cross); CHECK(c.r[AX]==0xbeef);
    c=execute(call,sizeof call); CHECK(c.r[AX]==2 && c.r[SP]==0x7c00);
    c=execute(illegal,sizeof illegal); CHECK(c.status==FD_X86_OPCODE && c.last_opcode==0x66);
    c=execute(loop,sizeof loop); CHECK(c.status==FD_X86_BUDGET && c.instructions==4096 && c.ip==0x7c00);
    c=execute(unsupported_bios,sizeof unsupported_bios); CHECK(c.status==FD_X86_BIOS);
    for(i=0;i<sizeof bpcode;++i) CHECK(fd_mem_write(0x7c00UL+i,bpcode[i])==0);
    CHECK(fd_mem_write(0x1100,0x34)==0 && fd_mem_write(0x1101,0x12)==0);
    CHECK(fd_mem_write(0x2100,0x78)==0 && fd_mem_write(0x2101,0x56)==0);
    fd_cpu_init(&c); c.r[BP]=0x100; c.s[SS]=0x100; c.s[DS]=0x200;
    CHECK(fd_cpu_run(&c,16,bios)==FD_X86_HALT && c.r[AX]==0x1234);
    CHECK(fd_mem_write(0xffff,0xb8)==0); CHECK(fd_mem_write(0,0x34)==0);
    CHECK(fd_mem_write(1,0x12)==0); CHECK(fd_mem_write(2,0xf4)==0);
    fd_cpu_init(&c); c.ip=0xffff;
    CHECK(fd_cpu_run(&c,16,bios)==FD_X86_HALT && c.r[AX]==0x1234 && c.ip==3);
    CHECK(fd_pager_flush()==0);
    fd_pager_init(read_page,write_page); fail_read=0x7c00/512;
    fd_cpu_init(&c); CHECK(fd_cpu_run(&c,16,bios)==FD_X86_MEMORY); fail_read=-1;
}
static void test_words(void) {
    uint16_t value; FDCPU c; unsigned i;
    static const uint32_t addresses[]={0x200,0x201,0x3ff,0x7ff,0xfffff};
    static const uint8_t code[]={0xa1,0xff,0xff,0x50,0x58,0xa3,0xff,0xff,0xf4};
    fd_pager_init(read_page,write_page);
    for(i=0;i<sizeof addresses/sizeof addresses[0];++i) {
        CHECK(fd_mem_write_word(addresses[i],0x81a5)==0);
        CHECK(fd_mem_read_word(addresses[i],&value)==0 && value==0x81a5);
        CHECK(fd_mem_get(addresses[i])==0xa5 && fd_mem_get(addresses[i]+1)==0x81);
    }
    CHECK(fd_pager_flush()==0); fd_pager_init(read_page,write_page);
    fail_read=1; CHECK(fd_mem_read_word(0x200,&value)==-1); fail_read=-1;
    fail_read=2; CHECK(fd_mem_read_word(0x3ff,&value)==-1); fail_read=-1;
    CHECK(fd_mem_write_word(0x3ff,0x1234)==0);
    CHECK(fd_pager_flush()==0); fd_pager_init(read_page,write_page);
    fail_read=2; CHECK(fd_mem_write_word(0x3ff,0x5678)==-1);
    CHECK(fd_mem_get(0x3ff)==0x78); fail_read=-1;
    CHECK(fd_mem_write_word(0xfe000,0x1234)==0);
    CHECK(fd_mem_write_word(0x9ffff,0xdead)==0);
    fd_mem_pc_map(1);
    CHECK(fd_mem_write_word(0xfe000,0xabcd)==0);
    CHECK(fd_mem_read_word(0xfe000,&value)==0 && value==0x1234);
    CHECK(fd_mem_read_word(0xa0000,&value)==0 && value==0xffff);
    CHECK(fd_mem_read_word(0x9ffff,&value)==0 && value==0xffad);
    CHECK(fd_mem_write_word(0x9ffff,0x5678)==0);
    CHECK(fd_mem_read_word(0x9ffff,&value)==0 && value==0xff78);
    fd_video_dirty=0;
    CHECK(fd_mem_write_word(0xb809f,0x0720)==0 && fd_video_dirty==3);
    fd_mem_pc_map(0);
    for(i=0;i<sizeof code;++i) CHECK(fd_mem_write(0x7c00+i,code[i])==0);
    /* DS offset FFFF wraps to DS:0000, not the next physical address. */
    CHECK(fd_mem_write(0x1ffff,0x34)==0 && fd_mem_write(0x10000,0x12)==0);
    fd_cpu_init(&c); c.s[DS]=0x1000; c.s[SS]=0x3000; c.r[SP]=1;
    CHECK(fd_cpu_run(&c,16,0)==FD_X86_HALT && c.r[AX]==0x1234 && c.r[SP]==1);
    CHECK(fd_mem_get(0x3ffff)==0x34 && fd_mem_get(0x30000)==0x12);
    CHECK(fd_pager_flush()==0);
}
static void test_disk(void) {
    uint32_t lba; uint8_t out[1024],in[1024],guard[512];
    CHECK(fd_chs_lba(0,0,1,&lba)==0 && lba==0);
    CHECK(fd_chs_lba(1023,3,16,&lba)==0 && lba==65535);
    CHECK(fd_chs_lba(1024,0,1,&lba)!=0);
    CHECK(fd_chs_lba(0,4,1,&lba)!=0); CHECK(fd_chs_lba(0,0,0,&lba)!=0);
    CHECK(fd_disk_range(65535,1)==0); CHECK(fd_disk_range(65535,2)!=0);
    CHECK(fd_disk_range(0xffffffffUL,2)!=0); CHECK(fd_disk_range(0,0)!=0);
    memset(out,0xa7,sizeof out);
    CHECK(fd_disk_write(write_sector,65534,2,out)==0);
    CHECK(fd_disk_read(read_sector,65534,2,in)==0 && memcmp(in,out,1024)==0);
    CHECK(fd_disk_write(write_sector,65535,2,out)!=0);
    CHECK(read_sector(0,guard)==0 && guard[0]==0); /* rejected range didn't wrap */
    CHECK(fflush(disk)==0);
}
static void test_extended_cpu(void) {
    FDCPU c; unsigned i; uint8_t v;
    static const uint8_t overlap[]={0xbe,0,3,0xbf,1,3,0xb9,1,4,0xf3,0xa4,0xf4};
    static const uint8_t fill[]={0xb8,0xef,0xbe,0xbf,0xff,1,0xb9,2,1,0xf3,0xab,0xf4};
    static const uint8_t mul[]={0xb8,0xff,0xff,0xbb,2,0,0xf7,0xe3,0xf4};
    static const uint8_t imul[]={0xb8,0,0x80,0xbb,0xff,0xff,0xf7,0xeb,0xf4};
    static const uint8_t div[]={0xb8,0x34,0x12,0xba,1,0,0xb9,0x10,0,0xf7,0xf1,0xf4};
    static const uint8_t idiv[]={0xb8,0xfb,0xff,0xba,0xff,0xff,0xbb,2,0,0xf7,0xfb,0xf4};
    static const uint8_t rcr[]={0xb0,1,0xf9,0xd0,0xd8,0xf4};
    static const uint8_t daa[]={0xb0,0x99,0x04,9,0x27,0xf4};
    static const uint8_t repeat[]={0xf3,0xaa,0xf4};
    static const uint8_t interrupt[]={0xcd,0x60,0xf4};
    CHECK(fd_mem_write(0x300,0x41)==0); c=execute(overlap,sizeof overlap);
    CHECK(c.status==FD_X86_HALT && c.r[CX]==0 && c.r[DI]==0x702);
    CHECK(fd_mem_read(0x701,&v)==0 && v==0x41);
    c=execute(fill,sizeof fill); CHECK(c.status==FD_X86_HALT && c.r[DI]==0x403);
    CHECK(fd_mem_read(511,&v)==0 && v==0xef); CHECK(fd_mem_read(512,&v)==0 && v==0xbe);
    CHECK(fd_mem_read(1026,&v)==0 && v==0xbe);
    c=execute(mul,sizeof mul); CHECK(c.r[AX]==0xfffe && c.r[DX]==1 && (c.flags&(FD_CF|FD_OF))==(FD_CF|FD_OF));
    c=execute(imul,sizeof imul); CHECK(c.r[AX]==0x8000 && c.r[DX]==0 && (c.flags&FD_OF));
    c=execute(div,sizeof div); CHECK(c.r[AX]==0x1123 && c.r[DX]==4);
    c=execute(idiv,sizeof idiv); CHECK(c.r[AX]==0xfffe && c.r[DX]==0xffff);
    c=execute(rcr,sizeof rcr); CHECK((c.r[AX]&255)==0x80 && (c.flags&FD_CF) && (c.flags&FD_OF));
    c=execute(daa,sizeof daa); CHECK((c.r[AX]&255)==8 && (c.flags&FD_CF) && (c.flags&FD_AF));
    for(i=0;i<sizeof repeat;++i) CHECK(fd_mem_write(0x7c00+i,repeat[i])==0);
    fd_cpu_init(&c); c.r[CX]=20; c.r[DI]=0x600; c.r[AX]=0x5a;
    CHECK(fd_cpu_run(&c,5,bios)==FD_X86_BUDGET && c.ip==0x7c00 && c.r[CX]==15 && c.r[DI]==0x605);
    CHECK(fd_cpu_run(&c,32,bios)==FD_X86_HALT && c.r[CX]==0 && c.r[DI]==0x614);
    CHECK(fd_mem_read(0x613,&v)==0 && v==0x5a);
    for(i=0;i<sizeof interrupt;++i) CHECK(fd_mem_write(0x7c00+i,interrupt[i])==0);
    CHECK(fd_mem_write(0x180,0)==0 && fd_mem_write(0x181,3)==0);
    CHECK(fd_mem_write(0x182,0)==0 && fd_mem_write(0x183,0)==0);
    CHECK(fd_mem_write(0x300,0x40)==0 && fd_mem_write(0x301,0xcf)==0);
    fd_cpu_init(&c); c.ivt_enabled=1; c.r[AX]=7; c.flags|=FD_CF|FD_IF;
    CHECK(fd_cpu_run(&c,32,0)==FD_X86_HALT && c.r[AX]==8 && c.r[SP]==0x7c00);
    CHECK((c.flags&(FD_CF|FD_IF))==(FD_CF|FD_IF));
}
static void test_movsw(void) {
    static const unsigned cases[][3]={{256,257,40},{257,260,70},{511,1537,257},
        {512,1536,257},{1022,2559,259},{768,512,257}};
    static const uint8_t code[]={0xf3,0xa5,0xf4};
    uint8_t expected[4096],lo,hi; unsigned k,j,s,d,n; FDCPU c;
    for(k=0;k<sizeof cases/sizeof cases[0];++k) {
        for(j=0;j<sizeof expected;++j) {
            expected[j]=(uint8_t)(j*31+(j>>8)); CHECK(fd_mem_write(0x1000UL+j,expected[j])==0);
        }
        s=cases[k][0]; d=cases[k][1]; n=cases[k][2];
        /* Independent word-at-a-time oracle, including overlapping odd addresses. */
        for(j=0;j<n;++j) { lo=expected[s]; hi=expected[s+1];
            expected[d]=lo; expected[d+1]=hi; s+=2; d+=2; }
        for(j=0;j<sizeof code;++j) CHECK(fd_mem_write(0x7c00UL+j,code[j])==0);
        fd_cpu_init(&c); c.r[SI]=0x1000+cases[k][0]; c.r[DI]=0x1000+cases[k][1]; c.r[CX]=n;
        while(fd_cpu_run(&c,17,0)==FD_X86_BUDGET) {}
        CHECK(c.status==FD_X86_HALT && c.r[CX]==0 && c.r[SI]==0x1000+s && c.r[DI]==0x1000+d);
        for(j=0;j<sizeof expected;++j) CHECK(fd_mem_get(0x1000UL+j)==expected[j]);
    }
    /* A word at FFFE is followed by a word at 0000, not physical 10000. */
    CHECK(fd_mem_write(0xfffe,0x12)==0 && fd_mem_write(0xffff,0x34)==0);
    CHECK(fd_mem_write(0,0x56)==0 && fd_mem_write(1,0x78)==0);
    fd_cpu_init(&c); c.r[SI]=0xfffe; c.r[DI]=0x800; c.r[CX]=2;
    CHECK(fd_cpu_run(&c,8,0)==FD_X86_HALT && c.r[SI]==2 && c.r[DI]==0x804);
    CHECK(fd_mem_get(0x800)==0x12 && fd_mem_get(0x801)==0x34 &&
        fd_mem_get(0x802)==0x56 && fd_mem_get(0x803)==0x78);
}
static void request(FDCPU *c,uint16_t ax,uint16_t cx,uint16_t dx,uint16_t es,uint16_t bx) {
    fd_cpu_init(c); c->r[AX]=ax; c->r[CX]=cx; c->r[DX]=dx;
    c->s[ES]=es; c->r[BX]=bx;
    CHECK(fd_bios_interrupt(c,0x13)==0);
}
static void test_bios(const char *fixture) {
    FDCPU c;
    uint8_t v,sector[512];
    FILE *saved;
    static const uint8_t truncated_int[]={0xcd};
    fd_bios_init(console_put,read_sector,write_sector);
    /* Two-sector transfer from the final cylinder, across cache pages. */
    request(&c,0x0202,0xffcf,0x0380,0x1000,0x0200);
    CHECK(!(c.flags&FD_CF) && c.r[AX]==2);
    CHECK(fd_mem_read(0x10200,&v)==0 && v==0xa7);
    CHECK(fd_mem_read(0x105ff,&v)==0 && v==0xa7);
    request(&c,0x0202,0xffd0,0x0380,0,0x600); CHECK(c.r[AX]==0x0400 && (c.flags&FD_CF));
    request(&c,0x0200,1,0x80,0,0x600); CHECK(c.flags&FD_CF);
    request(&c,0x0201,1,0x81,0,0x600); CHECK(c.r[AX]==0x0100 && (c.flags&FD_CF));
    request(&c,0x0201,1,0x80,1,0xffe0); CHECK(c.r[AX]==0x0900 && (c.flags&FD_CF));
    c.r[AX]=0x0100; CHECK(fd_bios_interrupt(&c,0x13)==0 && c.r[AX]==0x0900 && (c.flags&FD_CF));
    request(&c,0,0,0x80,0,0); CHECK(c.r[AX]==0 && !(c.flags&FD_CF));
    request(&c,0x0800,0,0x80,0,0); CHECK(c.r[CX]==0xffd0 && c.r[DX]==0x0301 && !(c.flags&FD_CF));
    fd_cpu_init(&c); CHECK(fd_bios_interrupt(&c,0x12)==0 && c.r[AX]==640);
    CHECK(fd_bios_interrupt(&c,0x21)!=0);
    fail_sector=65535;
    request(&c,0x0202,0xffcf,0x0380,0,0x600); CHECK(c.r[AX]==0x2001 && (c.flags&FD_CF));
    fail_sector=-1;
    CHECK(fd_mem_write(0x600,0x5a)==0);
    request(&c,0x0301,1,0x80,0,0x600); CHECK(c.r[AX]==1 && !(c.flags&FD_CF));
    CHECK(read_sector(0,sector)==0 && sector[0]==0x5a);
    fd_bios_init(console_put,read_sector,0);
    request(&c,0x0301,1,0x80,0,0x600); CHECK(c.r[AX]==0x0300 && (c.flags&FD_CF));
    CHECK(fd_pager_flush()==0);
    fd_pager_init(read_page,write_page); fail_read=3;
    request(&c,0x0201,1,0x80,0,0x600); CHECK(c.status==FD_X86_MEMORY);
    fail_read=-1;
    /* Fault while fetching INT's argument must not invoke the BIOS. */
    CHECK(fd_mem_write(0x7bff,truncated_int[0])==0); CHECK(fd_pager_flush()==0);
    fd_pager_init(read_page,write_page); fail_read=0x7c00/512;
    fd_cpu_init(&c); c.ip=0x7bff;
    CHECK(fd_cpu_run(&c,1,fd_bios_interrupt)==FD_X86_MEMORY); fail_read=-1;
    /* Boot a separately generated image, then execute its INT13 read. */
    saved=disk; disk=fopen(fixture,"rb"); CHECK(disk!=NULL);
    output_pos=0; output[0]=0;
    CHECK(fd_bios_boot(&c)==FD_BOOT_OK && c.ip==0x7c00 && c.r[DX]==0x80);
    CHECK(fd_cpu_run(&c,1024,fd_bios_interrupt)==FD_X86_HALT);
    CHECK(strcmp(output,"FD64 DISK BOOT OK")==0);
    CHECK(c.r[SI]==0x612 && !(c.flags&FD_CF));
    CHECK(fclose(disk)==0); disk=saved;
    CHECK(fd_bios_boot(&c)==FD_BOOT_SIGNATURE);
    fail_sector=0; CHECK(fd_bios_boot(&c)==FD_BOOT_IO); fail_sector=-1;
    fd_bios_init(console_put,0,0); CHECK(fd_bios_boot(&c)==FD_BOOT_IO);
}
static void probe_glabios(const char *path) {
    FDCPU c; uint8_t rom[8192]; unsigned i,checksum=0;
    FILE *stream=fopen(path,"rb"); CHECK(stream!=NULL);
    CHECK(fread(rom,1,sizeof rom,stream)==sizeof rom && fgetc(stream)==EOF);
    CHECK(fclose(stream)==0);
    for(i=0;i<sizeof rom;++i) {
        checksum+=rom[i];
        if(fd_mem_write(0xfe000UL+i,rom[i])) { CHECK(0); }
    }
    CHECK((checksum&255)==0 && rom[0x1ff0]==0xea);
    fd_cpu_reset(&c);
    CHECK(c.s[CS]==0xffff && c.ip==0 && c.r[SP]==0);
    CHECK(fd_cpu_run(&c,1,0)==FD_X86_BUDGET && c.s[CS]==0xf000 && c.ip==0xe05b);
    /* A capability probe, NOT a passing GLaBIOS POST test. */
    CHECK(fd_cpu_run(&c,128,0)==FD_X86_BIOS && c.last_opcode==0xe6);
    printf("GLABIOS_PROBE: CPU register/flags tests executed, STOP I/O %02X at %04X:%04X\n",
        c.last_opcode,c.s[CS],c.ip-1);
    CHECK(fd_mem_write(0xfffffUL,0x41)==0 && fd_pager_flush()==0);
}
static void test_pc_map(void) {
    fd_pager_init(read_page,write_page);
    CHECK(fd_mem_write(0xfe001UL,0x77)==0);
    CHECK(fd_mem_write(0xd0001UL,0xaa)==0);
    fd_mem_pc_map(1);
    CHECK(fd_mem_get(0xfe001UL)==0x77);
    CHECK(fd_mem_write(0xfe001UL,0x55)==0 && fd_mem_get(0xfe001UL)==0x77);
    CHECK(fd_mem_fill(0xfe000UL,0,128,0)==0 && fd_mem_get(0xfe001UL)==0x77);
    CHECK(fd_mem_copy(0xfe000UL,0,128)==0 && fd_mem_get(0xfe001UL)==0x77);
    CHECK(fd_mem_write(0xd0001UL,0x55)==0 && fd_mem_get(0xd0001UL)==0xaa);
    CHECK(fd_mem_get(0xa0000UL)==255 && fd_mem_get(0xc0000UL)==255);
    CHECK(fd_mem_write(0xa0000UL,0)==0 && fd_mem_get(0xa0000UL)==255);
    CHECK(fd_mem_copy(0x200,0xa0000UL,128)==0 && fd_mem_get(0x27f)==255);
    CHECK(fd_mem_write(0xb8000UL,0x41)==0 && fd_mem_get(0xb8000UL)==0x41 && fd_video_dirty==1);
    CHECK(fd_mem_fill(0xb8000UL+160,0x0720,160,1)==0 && fd_video_dirty==3);
    fd_mem_pc_map(0); CHECK(fd_pager_flush()==0);
}
static void test_byte_carry_exhaustive(void) {
    unsigned op,carry,a,b,bit,parity; int full,signed_full; uint16_t flags; uint8_t value; FDCPU c;
    for(op=0;op<2;++op) {
        CHECK(fd_mem_write(0x7c00UL,op?0x1c:0x14)==0); /* SBB / ADC AL, immediate */
        for(carry=0;carry<2;++carry) for(a=0;a<256;++a) for(b=0;b<256;++b) {
            fd_mem_write(0x7c01UL,(uint8_t)b); fd_cpu_init(&c); c.r[AX]=0x1200|a; c.flags|=carry;
            fd_cpu_run(&c,1,0);
            full=op?(int)a-(int)b-(int)carry:(int)a+(int)b+(int)carry;
            signed_full=(a<128?(int)a:(int)a-256)+(op?-1:1)*(b<128?(int)b:(int)b-256)+(op?-1:1)*(int)carry;
            value=(uint8_t)full; flags=0;
            if(full<0 || full>255) flags|=FD_CF;
            if(signed_full< -128 || signed_full>127) flags|=FD_OF;
            if(op?((a&15)<(b&15)+carry):((a&15)+(b&15)+carry>15)) flags|=FD_AF;
            if(!value) flags|=FD_ZF; if(value&128) flags|=FD_SF;
            for(bit=0,parity=0;bit<8;++bit) parity+=(value>>bit)&1;
            if(!(parity&1)) flags|=FD_PF;
            CHECK(c.r[AX]==(0x1200|value) &&
                (c.flags&(FD_CF|FD_OF|FD_AF|FD_ZF|FD_SF|FD_PF))==flags);
        }
    }
}
int main(int argc,char **argv) {
    char path[1024]; uint8_t v;
    CHECK(argc==4);
    snprintf(path,sizeof path,"%s/ram.bin",argv[1]); ram=fopen(path,"w+b"); CHECK(ram!=NULL);
    CHECK(fseek(ram,1048575,SEEK_SET)==0 && fputc(0,ram)!=EOF && fflush(ram)==0);
    snprintf(path,sizeof path,"%s/disk.bin",argv[1]); disk=fopen(path,"w+b"); CHECK(disk!=NULL);
    CHECK(fseek(disk,33554431,SEEK_SET)==0 && fputc(0,disk)!=EOF && fflush(disk)==0);
    test_pager(); test_cpu(); test_words(); test_disk(); test_extended_cpu(); test_movsw(); test_byte_carry_exhaustive(); test_bios(argv[2]); probe_glabios(argv[3]); test_pc_map();
    CHECK(fclose(ram)==0); CHECK(fclose(disk)==0);
    snprintf(path,sizeof path,"%s/ram.bin",argv[1]); ram=fopen(path,"rb"); CHECK(ram!=NULL);
    fd_pager_init(read_page,write_page);
    CHECK(fd_mem_read(0xfffffUL,&v)==0 && v==0x41); CHECK(fclose(ram)==0);
    printf("FD64_NATIVE_PASS: %d assertions\n",checks); return 0;
}
