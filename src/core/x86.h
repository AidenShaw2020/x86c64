/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FD_X86_H
#define FD_X86_H
#include <stdint.h>
#include <stddef.h>
enum { AX, CX, DX, BX, SP, BP, SI, DI };
enum { ES, CS, SS, DS };
enum { FD_X86_OK, FD_X86_HALT, FD_X86_OPCODE, FD_X86_MEMORY, FD_X86_BIOS, FD_X86_BUDGET, FD_X86_WAIT };
#define FD_CF 0x0001
#define FD_PF 0x0004
#define FD_AF 0x0010
#define FD_ZF 0x0040
#define FD_SF 0x0080
#define FD_TF 0x0100
#define FD_IF 0x0200
#define FD_DF 0x0400
#define FD_OF 0x0800
typedef struct {
    uint16_t r[8], s[4], ip, flags;
    uint8_t status, last_opcode, ivt_enabled;
    uint32_t instructions;
} FDCPU;
#ifdef __CC65__
/* The 6510 helpers use these fixed offsets. Fail the build if the ABI changes. */
typedef char FDCPU_asm_layout[(sizeof(FDCPU)==35 && offsetof(FDCPU,s)==16 &&
    offsetof(FDCPU,ip)==24 && offsetof(FDCPU,flags)==26 && offsetof(FDCPU,status)==28 &&
    offsetof(FDCPU,last_opcode)==29 && offsetof(FDCPU,instructions)==31)?1:-1];
#endif
typedef int (*fd_bios)(FDCPU *cpu, uint8_t interrupt);
typedef int (*fd_port_read)(uint16_t port, uint8_t *value);
typedef int (*fd_port_write)(uint16_t port, uint8_t value);
void fd_cpu_io(fd_port_read read_port, fd_port_write write_port);
void fd_cpu_interrupt(FDCPU *cpu, uint8_t interrupt);
void fd_cpu_init(FDCPU *cpu);
void fd_cpu_reset(FDCPU *cpu);
uint32_t fd_physical(uint16_t segment, uint16_t offset);
int fd_cpu_run(FDCPU *cpu, uint16_t budget, fd_bios bios);
#endif
