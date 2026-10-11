/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "pager.h"
#include <stddef.h>
#include <string.h>
typedef struct { uint16_t page; uint8_t valid, dirty, data[FD_PAGE_SIZE]; } Frame;
#ifdef __CC65__
Frame fd_cache_frames[FD_CACHE_PAGES];
#define frames fd_cache_frames
#else
static Frame frames[FD_CACHE_PAGES];
#endif
static uint8_t incoming[FD_PAGE_SIZE];
static unsigned next_frame;
#ifdef __CC65__
/* Assembly searches resident frames; page loads and replacement still enter C. */
Frame *fd_cache_frame;
uint8_t fd_pc_map;
#define current_frame fd_cache_frame
#define pc_map fd_pc_map
#define FD_GET_IMPL fd_mem_get_slow
#define FD_WRITE_IMPL fd_mem_write_slow
#define FD_READ_WORD_IMPL fd_mem_read_word_slow
#define FD_WRITE_WORD_IMPL fd_mem_write_word_slow
typedef char FDFrame_asm_layout[(FD_CACHE_PAGES==8 && FD_PAGE_SIZE==512 &&
    sizeof(Frame)==516 && offsetof(Frame,page)==0 && offsetof(Frame,valid)==2 &&
    offsetof(Frame,dirty)==3 && offsetof(Frame,data)==4)?1:-1];
#else
static Frame *current_frame;
static uint8_t pc_map;
#define FD_GET_IMPL fd_mem_get
#define FD_WRITE_IMPL fd_mem_write
#define FD_READ_WORD_IMPL fd_mem_read_word
#define FD_WRITE_WORD_IMPL fd_mem_write_word
#endif
static fd_page_read backend_read;
static fd_page_write backend_write;
uint32_t fd_video_dirty;
uint16_t fd_pager_generation;
void fd_mem_pc_map(uint8_t enable) {
    if(pc_map!=enable) { pc_map=enable; ++fd_pager_generation; }
}
static uint8_t writable(uint32_t address) {
    uint8_t bank=(uint8_t)(address>>16);
    uint16_t offset=(uint16_t)address;
    return !pc_map || bank<10 || (bank==11 && offset>=0x8000 && offset<0xc000);
}
static uint8_t readable(uint32_t address) {
    uint8_t bank=(uint8_t)(address>>16);
    uint16_t offset=(uint16_t)address;
    return !pc_map || bank<10 || (bank==15 && offset>=0xe000) ||
        (bank==11 && offset>=0x8000 && offset<0xc000) || (bank==13 && offset<0x800);
}
static void video_changed(uint32_t address,uint16_t bytes) {
    uint8_t row,last;
    if(address<0xb8000UL || address>=0xb8fa0UL) return;
    row=(uint8_t)((address-0xb8000UL)/160);
    last=(uint8_t)((address-0xb8000UL+bytes-1)/160);
    if(last>24) last=24;
    while(row<=last) fd_video_dirty|=1UL<<row++;
}
void fd_pager_init(fd_page_read r, fd_page_write w) {
    memset(frames, 0, sizeof frames);
    next_frame = 0; current_frame=0; backend_read = r; backend_write = w;
    pc_map=0; fd_video_dirty=0;
    ++fd_pager_generation;
}
static int frame_for(uint16_t page) {
    unsigned i;
    Frame *f;
    if(current_frame && current_frame->valid && current_frame->page==page) return 0;
    for (i = 0, f=frames; i < FD_CACHE_PAGES; ++i,++f)
        if (f->valid && f->page == page) { current_frame=f; return 0; }
    /* Read first: failed reads must not discard a resident dirty page. */
    if (backend_read(page, incoming)) return -1;
    f = &frames[next_frame];
    if (f->valid && f->dirty && backend_write(f->page, f->data)) return -1;
    f->page = page; f->valid = 1; f->dirty = 0;
    ++fd_pager_generation;
    memcpy(f->data, incoming, FD_PAGE_SIZE);
    current_frame=f;
    next_frame = (next_frame + 1) % FD_CACHE_PAGES;
    return 0;
}
int FD_GET_IMPL(uint32_t address) {
    int index;
    address &= 0xfffffUL;
    if(!readable(address)) return 255;
    index = frame_for((uint16_t)(address >> 9));
    if (index < 0) return -1;
    return current_frame->data[(unsigned)(address & 511)];
}
const uint8_t *fd_mem_span(uint32_t address,uint16_t *bytes) {
    static const uint8_t absent=255;
    address&=0xfffffUL;
    if(!readable(address)) { *bytes=1; return &absent; }
    if(frame_for((uint16_t)(address>>9))) return 0;
    *bytes=512-(unsigned)(address&511);
    return current_frame->data+(unsigned)(address&511);
}
int fd_mem_read(uint32_t address,uint8_t *value) {
    int result=fd_mem_get(address);
    if(result<0) return -1;
    *value=(uint8_t)result; return 0;
}
int FD_WRITE_IMPL(uint32_t address, uint8_t value) {
    int index;
    address &= 0xfffffUL;
    if(!writable(address)) return 0;
    index = frame_for((uint16_t)(address >> 9));
    if (index < 0) return -1;
    current_frame->data[(unsigned)(address & 511)] = value;
    current_frame->dirty = 1;
    if(pc_map && (uint8_t)(address>>16)==11) video_changed(address,1);
    return 0;
}
int FD_READ_WORD_IMPL(uint32_t address,uint16_t *value) {
    unsigned offset;
    int low,high;
    address&=0xfffffUL; offset=(unsigned)(address&511);
    if(offset==511) {
        low=fd_mem_get(address); if(low<0) return -1;
        high=fd_mem_get(address+1); if(high<0) return -1;
        *value=(uint16_t)low|((uint16_t)high<<8); return 0;
    }
    /* All PC memory-map boundaries are aligned to a cache page. */
    if(!readable(address)) { *value=0xffff; return 0; }
    if(frame_for((uint16_t)(address>>9))) return -1;
    *value=current_frame->data[offset]|((uint16_t)current_frame->data[offset+1]<<8);
    return 0;
}
int FD_WRITE_WORD_IMPL(uint32_t address,uint16_t value) {
    unsigned offset;
    address&=0xfffffUL; offset=(unsigned)(address&511);
    if(offset==511) {
        if(fd_mem_write(address,(uint8_t)value)) return -1;
        return fd_mem_write(address+1,(uint8_t)(value>>8));
    }
    if(!writable(address)) return 0;
    if(frame_for((uint16_t)(address>>9))) return -1;
    current_frame->data[offset]=(uint8_t)value;
    current_frame->data[offset+1]=(uint8_t)(value>>8);
    current_frame->dirty=1;
    if(pc_map && (uint8_t)(address>>16)==11) video_changed(address,2);
    return 0;
}
int fd_mem_copy(uint32_t destination,uint32_t source,uint16_t bytes) {
    Frame *target;
    uint8_t *from,*to;
    source&=0xfffffUL; destination&=0xfffffUL;
    if(!bytes || bytes>512-(source&511) || bytes>512-(destination&511)) return -1;
    if(!writable(destination)) return 0;
    if(!readable(source)) return fd_mem_fill(destination,255,bytes,0);
    if(frame_for((uint16_t)(source>>9))) return -1;
    if(frame_for((uint16_t)(destination>>9))) return -1;
    target=current_frame;
    /* Loading the destination may have evicted the source. */
    if(frame_for((uint16_t)(source>>9))) return -1;
    from=current_frame->data+(unsigned)(source&511); to=target->data+(unsigned)(destination&511);
    target->dirty=1;
    if(pc_map && (uint8_t)(destination>>16)==11) video_changed(destination,bytes);
    /* Forward byte order is essential for overlapping LZ decompression. */
    while(bytes--) *to++=*from++;
    return 0;
}
int fd_mem_fill(uint32_t destination,uint16_t value,uint16_t bytes,uint8_t word) {
    uint8_t *to;
    destination&=0xfffffUL;
    if(!bytes || bytes>512-(destination&511) || (word && (bytes&1))) return -1;
    if(!writable(destination)) return 0;
    if(frame_for((uint16_t)(destination>>9))) return -1;
    current_frame->dirty=1; to=current_frame->data+(unsigned)(destination&511);
    if(pc_map && (uint8_t)(destination>>16)==11) video_changed(destination,bytes);
    if(word) while(bytes) { *to++=(uint8_t)value; *to++=(uint8_t)(value>>8); bytes-=2; }
    else while(bytes--) *to++=(uint8_t)value;
    return 0;
}
int fd_pager_flush(void) {
    unsigned i;
    for (i = 0; i < FD_CACHE_PAGES; ++i) {
        if (frames[i].valid && frames[i].dirty) {
            if (backend_write(frames[i].page, frames[i].data)) return -1;
            frames[i].dirty = 0;
        }
    }
    return 0;
}
