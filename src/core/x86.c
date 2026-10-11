/* SPDX-License-Identifier: GPL-2.0-or-later
 * Instruction-level 8086 interpreter; timing is not cycle accurate.
 * Unsupported instructions stop; they are never silently treated as NOP.
 */
#include "x86.h"
#include "pager.h"
#include <string.h>
#ifdef __CC65__
/* One CPU instance runs at a time on C64. Avoid expensive software-stack locals. */
#define FD_LOCAL static
#else
#define FD_LOCAL
#endif
static fd_port_read port_read;
static fd_port_write port_write;
void fd_cpu_io(fd_port_read read_port, fd_port_write write_port) {
    port_read=read_port; port_write=write_port;
}
#ifndef __CC65__
uint32_t fd_physical(uint16_t seg, uint16_t off) {
    return (((uint32_t)seg << 4) + off) & 0xfffffUL;
}
#endif
void fd_cpu_init(FDCPU *c) {
    memset(c, 0, sizeof *c); c->flags = 2; c->ip = 0x7c00; c->r[SP] = 0x7c00;
}
void fd_cpu_reset(FDCPU *c) {
    memset(c,0,sizeof *c); c->flags=2; c->s[CS]=0xffff;
}
static uint8_t rb(FDCPU *c, uint16_t seg, uint16_t off) {
    int v=fd_mem_get(fd_physical(seg,off));
    if(v<0) { c->status=FD_X86_MEMORY; return 0; }
    return (uint8_t)v;
}
static void wb(FDCPU *c, uint16_t seg, uint16_t off, uint8_t v) {
    if (fd_mem_write(fd_physical(seg, off), v)) c->status = FD_X86_MEMORY;
}
static uint16_t rw(FDCPU *c,uint16_t seg,uint16_t off) {
    uint16_t value;
    /* Offset wraps inside the segment, independently of the physical bus. */
    if(off==0xffff) {
        value=rb(c,seg,off); return value|((uint16_t)rb(c,seg,0)<<8);
    }
    if(fd_mem_read_word(fd_physical(seg,off),&value)) { c->status=FD_X86_MEMORY; return 0; }
    return value;
}
static void ww(FDCPU *c,uint16_t seg,uint16_t off,uint16_t value) {
    if(off==0xffff) { wb(c,seg,off,(uint8_t)value); wb(c,seg,0,(uint8_t)(value>>8)); }
    else if(fd_mem_write_word(fd_physical(seg,off),value)) c->status=FD_X86_MEMORY;
}
#ifdef __CC65__
/* Shared with the 6510 fetch path. Only a refill enters the C pager. */
uint16_t fd_fetch_cs,fd_fetch_ip,fd_fetch_remaining,fd_fetch_generation;
uint16_t fd_fetch_start_ip,fd_fetch_length;
const uint8_t *fd_fetch_bytes;
const uint8_t *fd_fetch_start_bytes;
uint8_t __fastcall__ fd_cpu_fetch(FDCPU *c);
uint16_t __fastcall__ fd_cpu_fast(FDCPU *c,uint16_t budget);
uint8_t __fastcall__ fd_cpu_fetch_refill(FDCPU *c) {
    uint8_t value;
    fd_fetch_cs=c->s[CS];
    fd_fetch_bytes=fd_mem_span(fd_physical(fd_fetch_cs,c->ip),&fd_fetch_remaining);
    fd_fetch_generation=fd_pager_generation;
    if(!fd_fetch_bytes) { fd_fetch_remaining=fd_fetch_length=0; c->status=FD_X86_MEMORY; return 0; }
    if(c->ip && fd_fetch_remaining>(uint16_t)(0-c->ip)) fd_fetch_remaining=(uint16_t)(0-c->ip);
    fd_fetch_start_ip=c->ip; fd_fetch_start_bytes=fd_fetch_bytes; fd_fetch_length=fd_fetch_remaining;
    value=*fd_fetch_bytes++; --fd_fetch_remaining;
    ++c->ip; fd_fetch_ip=c->ip;
    if(!fd_fetch_ip) fd_fetch_remaining=0;
    return value;
}
#define fetch fd_cpu_fetch
#else
static uint8_t fetch(FDCPU *c) {
    static uint16_t cached_cs,next_ip,remaining,generation;
    static uint32_t cached_base;
    static const uint8_t *bytes;
    uint8_t v;
    if(cached_cs!=c->s[CS]) { cached_cs=c->s[CS]; cached_base=(uint32_t)cached_cs<<4; remaining=0; }
    if(!remaining || next_ip!=c->ip || generation!=fd_pager_generation) {
        bytes=fd_mem_span(cached_base+c->ip,&remaining); generation=fd_pager_generation;
        if(!bytes) { c->status=FD_X86_MEMORY; return 0; }
    }
    v=*bytes++; --remaining;
    c->ip++;
    next_ip=c->ip;
    if(!next_ip) remaining=0; /* A 16-bit IP wrap is not a linear physical address. */
    return v;
}
#endif
static uint16_t fw(FDCPU *c) {
    uint16_t v = fetch(c); return v | ((uint16_t)fetch(c) << 8);
}
/* cc65 does not implement signed 8-bit arithmetic like host compilers. */
static uint16_t displacement8(uint8_t v) {
    return (v & 0x80) ? (uint16_t)(0xff00u | v) : (uint16_t)v;
}
static uint8_t r8(FDCPU *c, unsigned r) {
    return (uint8_t)((r&4) ? c->r[r&3]>>8:c->r[r&3]);
}
static void w8(FDCPU *c, unsigned r, uint8_t v) {
    if(r&4) c->r[r&3]=(c->r[r&3]&255)|((uint16_t)v<<8);
    else c->r[r&3]=(c->r[r&3]&0xff00)|v;
}
#ifdef __CC65__
void __fastcall__ fd_cpu_szp(FDCPU *c,uint16_t v,int word);
uint16_t __fastcall__ fd_cpu_alu(FDCPU *c,unsigned op,uint16_t a,uint16_t b,int word);
#define szp fd_cpu_szp
#define alu fd_cpu_alu
#else
static void szp(FDCPU *c, uint16_t v, int word) {
    uint8_t b = (uint8_t)v;
    c->flags &= ~(FD_SF | FD_ZF | FD_PF);
    if (!v) c->flags |= FD_ZF;
    if (v & (word ? 0x8000 : 0x80)) c->flags |= FD_SF;
    b ^= b >> 4; b ^= b >> 2; b ^= b >> 1;
    if (!(b & 1)) c->flags |= FD_PF;
}
static uint16_t alu(FDCPU *c, unsigned op, uint16_t a, uint16_t b, int word) {
    uint16_t full;
    uint16_t v, sign = word ? 0x8000 : 0x80;
    unsigned carry = (c->flags & FD_CF) ? 1 : 0;
    c->flags &= ~(FD_CF | FD_OF | FD_AF);
    if (op == 1 || op == 4 || op == 6) {
        v = op == 1 ? a | b : (op == 4 ? a & b : a ^ b);
    } else {
        if (op != 2 && op != 3) carry = 0;
        if (op == 0 || op == 2) {
            full=(uint16_t)(a+b); v=(uint16_t)(full+carry);
            if(full<a || v<full || (!word && v>255)) c->flags|=FD_CF;
            if(!word) v&=255;
            if ((~(a ^ b) & (a ^ v) & sign)) c->flags |= FD_OF;
        } else {
            full=(uint16_t)(b+carry); v=(uint16_t)(a-full);
            if(full<b || a<full) c->flags|=FD_CF;
            if(!word) v&=255;
            if (((a ^ b) & (a ^ v) & sign)) c->flags |= FD_OF;
        }
        if ((a ^ b ^ v) & 16) c->flags |= FD_AF;
    }
    szp(c, v, word); return v;
}
#endif
typedef struct { unsigned reg; uint16_t seg, off; int memory; } Operand;
static void operand(FDCPU *c, uint8_t m, int override, Operand *out) {
    Operand o;
    unsigned mode = m >> 6, rm = m & 7;
    int use_ss = 0;
    o.reg = rm; o.memory = mode != 3; o.off = 0; o.seg = c->s[DS];
    if (!o.memory) { *out = o; return; }
    switch (rm) {
    case 0: o.off = c->r[BX] + c->r[SI]; break;
    case 1: o.off = c->r[BX] + c->r[DI]; break;
    case 2: o.off = c->r[BP] + c->r[SI]; use_ss = 1; break;
    case 3: o.off = c->r[BP] + c->r[DI]; use_ss = 1; break;
    case 4: o.off = c->r[SI]; break;
    case 5: o.off = c->r[DI]; break;
    case 6: if (!mode) o.off = fw(c); else { o.off = c->r[BP]; use_ss = 1; } break;
    case 7: o.off = c->r[BX]; break;
    }
    if (mode == 1) o.off += displacement8(fetch(c));
    if (mode == 2) o.off += fw(c);
    o.seg = c->s[override >= 0 ? override : (use_ss ? SS : DS)];
    *out = o;
}
static uint16_t get(FDCPU *c, Operand *o, int word) {
    if (!o->memory) return word ? c->r[o->reg] : r8(c, o->reg);
    return word ? rw(c,o->seg,o->off) : rb(c,o->seg,o->off);
}
static void put(FDCPU *c, Operand *o, int word, uint16_t v) {
    if (!o->memory) { if (word) c->r[o->reg] = v; else w8(c, o->reg, (uint8_t)v); }
    else {
        if(word) ww(c,o->seg,o->off,v); else wb(c,o->seg,o->off,(uint8_t)v);
    }
}
static void push(FDCPU *c, uint16_t v) {
    c->r[SP] -= 2; ww(c,c->s[SS],c->r[SP],v);
}
static uint16_t pop(FDCPU *c) {
    uint16_t v = rw(c,c->s[SS],c->r[SP]);
    c->r[SP] += 2; return v;
}
void fd_cpu_interrupt(FDCPU *c, uint8_t n) {
    uint16_t address=(uint16_t)n*4, target, segment;
    if(!c->ivt_enabled) { c->status=FD_X86_BIOS; return; }
    if(c->status==FD_X86_BUDGET || c->status==FD_X86_HALT) c->status=FD_X86_OK;
    push(c,c->flags|0xf002); push(c,c->s[CS]); push(c,c->ip);
    target=rw(c,0,address); segment=rw(c,0,address+2);
    if(c->status!=FD_X86_OK) return;
    c->ip=target; c->s[CS]=segment; c->flags&=~(FD_IF|FD_TF);
}
static void software_interrupt(FDCPU *c, uint8_t n, fd_bios bios) {
    int result=bios ? bios(c,n) : 1;
    if(result<0) { if(c->status==FD_X86_OK) c->status=FD_X86_BIOS; }
    else if(result>0) fd_cpu_interrupt(c,n);
}
static uint16_t shift(FDCPU *c, uint16_t value, uint8_t op, uint8_t count, uint8_t word) {
    uint16_t mask=word ? 0xffff:0xff, sign=word ? 0x8000:0x80, original=value;
    uint8_t n=count, carry, previous;
    if(!count) return value;
    while(n--) {
        previous=!!(c->flags&FD_CF);
        if(op==0 || op==2 || op==4 || op==6) {
            carry=!!(value&sign); value=(value<<1)&mask;
            if(op==0) value|=carry; else if(op==2) value|=previous;
        } else {
            carry=value&1; value>>=1;
            if(op==1) { if(carry) value|=sign; }
            else if(op==3) { if(previous) value|=sign; }
            else if(op==7 && (original&sign)) value|=sign;
        }
        c->flags=(c->flags&~FD_CF)|(carry ? FD_CF:0);
    }
    if(op>=4) szp(c,value,word);
    if(count==1) {
        c->flags&=~FD_OF;
        if(op==0 || op==2 || op==4 || op==6) {
            if(!!(value&sign)!=!!(c->flags&FD_CF)) c->flags|=FD_OF;
        } else if(op==1 || op==3) {
            if(!!(value&sign)!=!!(value&(sign>>1))) c->flags|=FD_OF;
        } else if(op==5 && (original&sign)) c->flags|=FD_OF;
    }
    return value;
}
static int32_t signed16(uint16_t v) {
    return (v&0x8000) ? (int32_t)v-65536L : (int32_t)v;
}
static void group3(FDCPU *c, Operand *o, uint8_t reg, uint8_t word, fd_bios bios) {
    uint16_t value=get(c,o,word), mask=word ? 0xffff:0xff, imm;
    uint32_t product, dividend, divisor, quotient, remainder;
    int32_t signed_product, factor;
    uint8_t negative, divisor_negative;
    if(c->status!=FD_X86_OK) return;
    switch(reg) {
    case 0: imm=word ? fw(c):fetch(c); alu(c,4,value,imm,word); return;
    case 1: c->status=FD_X86_OPCODE; return;
    case 2: put(c,o,word,value^mask); return;
    case 3: put(c,o,word,alu(c,5,0,value,word)); return;
    case 4:
        product=(uint32_t)(word ? c->r[AX]:r8(c,0))*value;
        c->r[AX]=(uint16_t)product;
        if(word) c->r[DX]=(uint16_t)(product>>16);
        c->flags&=~(FD_CF|FD_OF);
        if(product>(word ? 65535UL:255UL)) c->flags|=FD_CF|FD_OF;
        return;
    case 5:
        factor=word ? signed16(value):(int32_t)(int16_t)displacement8((uint8_t)value);
        signed_product=(word ? signed16(c->r[AX]):(int32_t)(int16_t)displacement8(r8(c,0)))*factor;
        c->r[AX]=(uint16_t)signed_product;
        if(word) c->r[DX]=(uint16_t)((uint32_t)signed_product>>16);
        c->flags&=~(FD_CF|FD_OF);
        if(signed_product<(word ? -32768L:-128L) || signed_product>(word ? 32767L:127L))
            c->flags|=FD_CF|FD_OF;
        return;
    default:
        dividend=word ? ((uint32_t)c->r[DX]<<16)|c->r[AX] : c->r[AX];
        divisor=value; negative=0; divisor_negative=0;
        if(reg==7) {
            negative=word ? !!(c->r[DX]&0x8000):!!(c->r[AX]&0x8000);
            divisor_negative=!!(value&(word ? 0x8000:0x80));
            if(negative) dividend=word ? 0UL-dividend:(uint16_t)(0-c->r[AX]);
            if(divisor_negative) divisor=(0UL-value)&mask;
        }
        if(!divisor) { software_interrupt(c,0,bios); return; }
        quotient=dividend/divisor; remainder=dividend%divisor;
        if(reg==6 ? quotient>mask : quotient>((negative!=divisor_negative) ?
            (word ? 32768UL:128UL):(word ? 32767UL:127UL))) {
            software_interrupt(c,0,bios); return;
        }
        if(reg==7) {
            if(negative!=divisor_negative) quotient=0UL-quotient;
            if(negative) remainder=0UL-remainder;
        }
        if(word) { c->r[AX]=(uint16_t)quotient; c->r[DX]=(uint16_t)remainder; }
        else { w8(c,0,(uint8_t)quotient); w8(c,4,(uint8_t)remainder); }
    }
}
static int condition(FDCPU *c, unsigned n) {
    int cf = !!(c->flags & FD_CF), zf = !!(c->flags & FD_ZF);
    int sf = !!(c->flags & FD_SF), of = !!(c->flags & FD_OF);
    int yes = 0;
    switch (n >> 1) {
    case 0: yes = of; break; case 1: yes = cf; break;
    case 2: yes = zf; break; case 3: yes = cf || zf; break;
    case 4: yes = sf; break; case 5: yes = !!(c->flags & FD_PF); break;
    case 6: yes = sf != of; break; case 7: yes = zf || (sf != of); break;
    }
    return (n & 1) ? !yes : yes;
}
int fd_cpu_run(FDCPU *context, uint16_t budget, fd_bios bios) {
    FD_LOCAL FDCPU *c;
    FD_LOCAL uint16_t i, a, b, v, oldcf, off, start, segment;
    FD_LOCAL uint8_t op, m, reg, word, dir, repeat, iterations, again;
    FD_LOCAL int override, prefixes;
    FD_LOCAL Operand o, dest;
    FD_LOCAL uint32_t source_address,destination_address;
    c=context;
    if (c->status != FD_X86_OK && c->status != FD_X86_BUDGET && c->status != FD_X86_WAIT) return c->status;
    c->status = FD_X86_OK;
    for (i = 0; i < budget && c->status == FD_X86_OK; ++i) {
#ifdef __CC65__
        i+=fd_cpu_fast(c,budget-i);
        if(i>=budget || c->status!=FD_X86_OK) break;
#endif
        override = -1; prefixes = 0; repeat=0; start=c->ip;
        do {
            op = fetch(c);
            if (op == 0x26) override = ES; else if (op == 0x2e) override = CS;
            else if (op == 0x36) override = SS; else if (op == 0x3e) override = DS;
            else if (op == 0xf2 || op == 0xf3) repeat=op;
            else if (op == 0xf0) { /* LOCK: single CPU, no concurrent bus master. */ }
            else break;
            if (++prefixes > 15) { c->status = FD_X86_OPCODE; break; }
        } while (c->status == FD_X86_OK);
        if (c->status != FD_X86_OK) break;
        c->last_opcode = op; c->instructions++;
        if (op >= 0xb0 && op <= 0xb7) { w8(c, op-0xb0, fetch(c)); continue; }
        if (op >= 0xb8 && op <= 0xbf) { c->r[op-0xb8] = fw(c); continue; }
        if (op >= 0x50 && op <= 0x57) {
            reg = op-0x50; v = c->r[reg]; if (reg == SP) v -= 2; push(c, v); continue;
        }
        if (op >= 0x58 && op <= 0x5f) { c->r[op-0x58] = pop(c); continue; }
        if (op >= 0x40 && op <= 0x4f) {
            reg = op & 7; oldcf = c->flags & FD_CF;
            c->r[reg] = alu(c, (op & 8) ? 5 : 0, c->r[reg], 1, 1);
            c->flags = (c->flags & ~FD_CF) | oldcf; continue;
        }
        if (op >= 0x70 && op <= 0x7f) {
            b = displacement8(fetch(c));
            if (condition(c, op & 15)) c->ip += b; continue;
        }
        if(op>=0x91 && op<=0x97) { v=c->r[AX]; c->r[AX]=c->r[op-0x90]; c->r[op-0x90]=v; continue; }
        /* Register/memory arithmetic 00h..3Bh; accumulator immediates. */
        if (op < 0x40 && (op & 7) <= 5) {
            reg = op >> 3; word = op & 1;
            if ((op & 7) <= 3) {
                m = fetch(c); operand(c, m, override, &o); dir = (op >> 1) & 1;
                dest.reg = (m >> 3) & 7; dest.memory = 0;
                a = dir ? get(c, &dest, word) : get(c, &o, word);
                b = dir ? get(c, &o, word) : get(c, &dest, word);
                v = alu(c, reg, a, b, word);
                if (reg != 7) put(c, dir ? &dest : &o, word, v);
            } else {
                a = word ? c->r[AX] : r8(c, 0); b = word ? fw(c) : fetch(c);
                v = alu(c, reg, a, b, word);
                if (reg != 7) { if (word) c->r[AX] = v; else w8(c, 0, (uint8_t)v); }
            }
            continue;
        }
        switch (op) {
        case 0x06: case 0x0e: case 0x16: case 0x1e: push(c,c->s[op>>3]); break;
        case 0x07: case 0x17: case 0x1f: c->s[op>>3]=pop(c); break;
        case 0x27: case 0x2f:
            a=r8(c,0); oldcf=c->flags&FD_CF; b=c->flags&FD_AF; v=a;
            c->flags&=~(FD_CF|FD_AF);
            if((a&15)>9 || b) {
                v=op==0x27 ? a+6:a-6; c->flags|=FD_AF;
                if(op==0x2f && (a<6 || oldcf)) c->flags|=FD_CF;
            }
            if(a>0x99 || oldcf) { v=op==0x27 ? v+0x60:v-0x60; c->flags|=FD_CF; }
            w8(c,0,(uint8_t)v); szp(c,r8(c,0),0); break;
        case 0x37: case 0x3f:
            a=c->r[AX]; b=c->flags&FD_AF; c->flags&=~(FD_AF|FD_CF);
            if((a&15)>9 || b) { c->r[AX]=(op==0x37) ? a+0x106:a-0x106; c->flags|=FD_AF|FD_CF; }
            c->r[AX]&=0xff0f; break;
        case 0x80: case 0x81: case 0x82: case 0x83:
            word=op&1; m=fetch(c); operand(c,m,override,&o); reg=(m>>3)&7;
            b=op==0x81 ? fw(c):(op==0x83 ? displacement8(fetch(c)):fetch(c));
            a=get(c,&o,word); v=alu(c,reg,a,b,word); if(reg!=7) put(c,&o,word,v); break;
        case 0x86: case 0x87:
            word=op&1; m=fetch(c); operand(c,m,override,&o); reg=(m>>3)&7;
            a=get(c,&o,word); b=word ? c->r[reg]:r8(c,reg); put(c,&o,word,b);
            if(word) c->r[reg]=a; else w8(c,reg,(uint8_t)a); break;
        case 0x88: case 0x89: case 0x8a: case 0x8b:
            word = op & 1; dir = (op >> 1) & 1; m = fetch(c);
            operand(c, m, override, &o); dest.memory = 0; dest.reg = (m >> 3) & 7;
            v = get(c, dir ? &o : &dest, word); put(c, dir ? &dest : &o, word, v); break;
        case 0x8c: case 0x8e:
            m = fetch(c); reg = (m >> 3) & 7; operand(c, m, override, &o);
            if (reg > 3 || (op == 0x8e && reg == CS)) { c->status = FD_X86_OPCODE; break; }
            if (op == 0x8e) c->s[reg] = get(c, &o, 1); else put(c, &o, 1, c->s[reg]); break;
        case 0x84: case 0x85:
            word = op & 1; m = fetch(c); operand(c, m, override, &o);
            a = get(c, &o, word); b = word ? c->r[(m >> 3)&7] : r8(c, (m >> 3)&7);
            alu(c, 4, a, b, word); break;
        case 0xc6: case 0xc7:
            word = op & 1; m = fetch(c); operand(c, m, override, &o);
            if (m & 0x38) { c->status = FD_X86_OPCODE; break; }
            v = word ? fw(c) : fetch(c); put(c, &o, word, v); break;
        case 0x90: break;
        case 0x8d:
            m=fetch(c); operand(c,m,override,&o);
            if(!o.memory) c->status=FD_X86_OPCODE; else c->r[(m>>3)&7]=o.off; break;
        case 0x8f:
            m=fetch(c); operand(c,m,override,&o);
            if(m&0x38) c->status=FD_X86_OPCODE; else put(c,&o,1,pop(c)); break;
        case 0x98: c->r[AX]=displacement8(r8(c,0)); break;
        case 0x99: c->r[DX]=(c->r[AX]&0x8000) ? 0xffff:0; break;
        case 0x9a:
            off=fw(c); segment=fw(c); push(c,c->s[CS]); push(c,c->ip);
            c->s[CS]=segment; c->ip=off; break;
        case 0x9b: break; /* no 8087 installed */
        case 0x9c: push(c, c->flags | 0xf002); break;
        case 0x9d: c->flags = (pop(c) & 0x0fd5) | 2; break;
        case 0x9e: c->flags=(c->flags&0xff00)|(r8(c,4)&0xd5)|2; break;
        case 0x9f: w8(c,4,(uint8_t)(c->flags|2)); break;
        case 0xa8: case 0xa9:
            word=op&1; a=word ? c->r[AX]:r8(c,0); b=word ? fw(c):fetch(c); alu(c,4,a,b,word); break;
        case 0xa0: case 0xa1: case 0xa2: case 0xa3:
            word = op & 1; o.memory = 1; o.off = fw(c); o.seg = c->s[override < 0 ? DS : override];
            if (op & 2) put(c, &o, word, word ? c->r[AX] : r8(c,0));
            else { v = get(c, &o, word); if (word) c->r[AX] = v; else w8(c,0,(uint8_t)v); } break;
        case 0xa4: case 0xa5: case 0xa6: case 0xa7:
        case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
            if(repeat && !c->r[CX]) break;
            word=op&1; off=(c->flags&FD_DF) ? (uint16_t)(0-(word+1)):word+1;
            o.memory=1; o.seg=c->s[override<0 ? DS:override];
            dest.memory=1; dest.seg=c->s[ES]; iterations=0;
            if(repeat && !(c->flags&FD_DF) && (op==0xa4 || op==0xa5 || op==0xaa || op==0xab)) {
                destination_address=fd_physical(dest.seg,c->r[DI]);
                a=(uint16_t)((512-(destination_address&511))>>word);
                if(op==0xa4 || op==0xa5) {
                    source_address=fd_physical(o.seg,c->r[SI]);
                    b=(uint16_t)((512-(source_address&511))>>word); if(a>b) a=b;
                    if(c->r[SI] && a>((uint16_t)(0-c->r[SI])>>word)) a=(uint16_t)(0-c->r[SI])>>word;
                    /* MOVSW reads both source bytes before either destination byte is written. */
                    if(word && ((destination_address-source_address)&0xfffffUL)==1) a=0;
                }
                if(c->r[DI] && a>((uint16_t)(0-c->r[DI])>>word)) a=(uint16_t)(0-c->r[DI])>>word;
                if(a>c->r[CX]) a=c->r[CX]; if(a>budget-i) a=budget-i;
                if(a>1) {
                    if(op<=0xa5 ? fd_mem_copy(destination_address,source_address,a<<word) :
                        fd_mem_fill(destination_address,c->r[AX],a<<word,word)) {
                        c->status=FD_X86_MEMORY; break;
                    }
                    if(op<=0xa5) c->r[SI]+=a<<word;
                    c->r[DI]+=a<<word; c->r[CX]-=a;
                    c->instructions+=a-1; i+=a-1;
                    if(c->r[CX]) c->ip=start;
                    break;
                }
            }
            do {
                o.off=c->r[SI]; dest.off=c->r[DI];
                if(op<=0xa7 || op==0xac || op==0xad) {
                    a=get(c,&o,word); c->r[SI]+=off;
                } else a=word ? c->r[AX]:r8(c,0);
                if(op==0xa4 || op==0xa5 || op==0xaa || op==0xab) put(c,&dest,word,a);
                else if(op==0xac || op==0xad) { if(word) c->r[AX]=a; else w8(c,0,(uint8_t)a); }
                else alu(c,7,a,get(c,&dest,word),word);
                if(op!=0xac && op!=0xad) c->r[DI]+=off;
                again=0;
                if(repeat && --c->r[CX]) {
                    again=1;
                    if(op==0xa6 || op==0xa7 || op==0xae || op==0xaf)
                        again=!!(c->flags&FD_ZF)==(repeat==0xf3);
                }
                if(!again || ++iterations==32 || i+1>=budget || c->status!=FD_X86_OK) break;
                ++i; ++c->instructions;
            } while(1);
            if(again) c->ip=start;
            break;
        case 0xc4: case 0xc5:
            m=fetch(c); operand(c,m,override,&o);
            if(!o.memory) { c->status=FD_X86_OPCODE; break; }
            c->r[(m>>3)&7]=get(c,&o,1); o.off+=2; c->s[op==0xc4 ? ES:DS]=get(c,&o,1); break;
        case 0xe8:
            off = fw(c); push(c, c->ip); c->ip += off; break;
        case 0xc2: case 0xc3:
            off=op==0xc2 ? fw(c):0; c->ip=pop(c); c->r[SP]+=off; break;
        case 0xca: case 0xcb:
            off=op==0xca ? fw(c):0; c->ip=pop(c); c->s[CS]=pop(c); c->r[SP]+=off; break;
        case 0xcc: software_interrupt(c,3,bios); break;
        case 0xce: if(c->flags&FD_OF) software_interrupt(c,4,bios); break;
        case 0xcf: c->ip=pop(c); c->s[CS]=pop(c); c->flags=(pop(c)&0x0fd5)|2; break;
        case 0xd0: case 0xd1: case 0xd2: case 0xd3:
            word=op&1; m=fetch(c); operand(c,m,override,&o); a=get(c,&o,word);
            put(c,&o,word,shift(c,a,(m>>3)&7,(op&2) ? r8(c,1):1,word)); break;
        case 0xd4:
            b=fetch(c); a=r8(c,0); if(!b) { software_interrupt(c,0,bios); break; }
            w8(c,4,(uint8_t)(a/b)); w8(c,0,(uint8_t)(a%b)); szp(c,r8(c,0),0); break;
        case 0xd5:
            b=fetch(c); a=(uint8_t)(r8(c,0)+r8(c,4)*b); c->r[AX]=a; szp(c,a,0); break;
        case 0xd7: w8(c,0,rb(c,c->s[override<0 ? DS:override],c->r[BX]+r8(c,0))); break;
        case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
            m=fetch(c); operand(c,m,override,&o); /* ESC, no coprocessor */ break;
        case 0xe9: off = fw(c); c->ip += off; break;
        case 0xea:
            off = fw(c); v = fw(c);
            if (c->status == FD_X86_OK) { c->ip = off; c->s[CS] = v; }
            break;
        case 0xeb: off = displacement8(fetch(c)); c->ip += off; break;
        case 0xe2: off = displacement8(fetch(c)); if (--c->r[CX]) c->ip += off; break;
        case 0xe0: case 0xe1:
            off=displacement8(fetch(c));
            if(--c->r[CX] && !!(c->flags&FD_ZF)==(op==0xe1)) c->ip+=off; break;
        case 0xe3: off = displacement8(fetch(c)); if (!c->r[CX]) c->ip += off; break;
        case 0xcd:
            m = fetch(c);
            if (c->status == FD_X86_OK) software_interrupt(c,m,bios);
            break;
        case 0xe4: case 0xe5: case 0xec: case 0xed:
            off=(op&8) ? c->r[DX]:fetch(c); a=0; m=0;
            if(!port_read || port_read(off,&m)) { c->status=FD_X86_BIOS; break; }
            a=m;
            if(op&1) { if(port_read(off+1,&m)) { c->status=FD_X86_BIOS; break; } c->r[AX]=a|((uint16_t)m<<8); }
            else w8(c,0,(uint8_t)a); break;
        case 0xe6: case 0xe7: case 0xee: case 0xef:
            off=(op&8) ? c->r[DX]:fetch(c);
            if(!port_write || port_write(off,r8(c,0)) || ((op&1) && port_write(off+1,r8(c,4))))
                c->status=FD_X86_BIOS;
            break;
        case 0xf4: c->status = FD_X86_HALT; break;
        case 0xf5: c->flags^=FD_CF; break;
        case 0xf6: case 0xf7:
            word=op&1; m=fetch(c); operand(c,m,override,&o); group3(c,&o,(m>>3)&7,word,bios); break;
        case 0xf8: c->flags &= ~FD_CF; break;
        case 0xf9: c->flags |= FD_CF; break;
        case 0xfa: c->flags &= ~FD_IF; break;
        case 0xfb: c->flags |= FD_IF; break;
        case 0xfc: c->flags &= ~FD_DF; break;
        case 0xfd: c->flags |= FD_DF; break;
        case 0xfe: case 0xff:
            word=op&1; m=fetch(c); operand(c,m,override,&o); reg=(m>>3)&7; a=get(c,&o,word);
            if(reg<2) {
                oldcf=c->flags&FD_CF; v=alu(c,reg ? 5:0,a,1,word);
                c->flags=(c->flags&~FD_CF)|oldcf; put(c,&o,word,v);
            } else if(!word || reg==7) c->status=FD_X86_OPCODE;
            else if(reg==6) { if(!o.memory && o.reg==SP) a-=2; push(c,a); }
            else if(reg==2 || reg==4) { if(reg==2) push(c,c->ip); c->ip=a; }
            else {
                if(!o.memory) { c->status=FD_X86_OPCODE; break; }
                o.off+=2; segment=get(c,&o,1);
                if(reg==3) { push(c,c->s[CS]); push(c,c->ip); }
                c->s[CS]=segment; c->ip=a;
            }
            break;
        default: c->status = FD_X86_OPCODE; break;
        }
    }
    if (c->status == FD_X86_OK) c->status = FD_X86_BUDGET;
    return c->status;
}
