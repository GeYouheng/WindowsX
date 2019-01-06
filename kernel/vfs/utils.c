#include <zjunix/vfs/vfs.h>
#include <zjunix/utils.h>
#include <driver/sd.h>
#include <zjunix/log.h>
#include <driver/vga.h>

u32 read_block(u8 *buf, u32 addr, u32 count) {
    return sd_read_block(buf, addr, count);
}

u32 write_block(u8 *buf, u32 addr, u32 count) {
    return sd_write_block(buf, addr, count);
}

u16 get_u16(u8 *ch) {
    return (*ch) + ((*(ch + 1)) << 8);
}

u32 get_u32(u8 *ch) {
    return (*ch) + ((*(ch + 1)) << 8) + ((*(ch + 2)) << 16) + ((*(ch + 3)) << 24);
}

void set_u16(u8 *ch, u16 num) {
    *ch = (u8)(num & 0xFF);
    *(ch + 1) = (u8)((num >> 8) & 0xFF);
}

void set_u32(u8 *ch, u32 num) {
    *ch = (u8)(num & 0xFF);
    *(ch + 1) = (u8)((num >> 8) & 0xFF);
    *(ch + 2) = (u8)((num >> 16) & 0xFF);
    *(ch + 3) = (u8)((num >> 24) & 0xFF);
}

u32 generic_compare_filename(const struct qstr *a, const struct qstr *b){
    u32 i;
    
    if (a->len == b->len) {
        for (i = 0; i < a->len; i++)
            if (a->name[i] != b->name[i])
                return 1;
    }
    else
        return 1;    
    
	return 0;
}

u32 get_bit(const u8 *bitmap, u32 index){
    const u8 *byte;
    u8 mask;
    
    byte = bitmap + index / BITS_PER_BYTE;
    mask = 1 << (index % BITS_PER_BYTE);
    
    return (u32)(*byte & mask) != 0;
}

void set_bit(u8 *bitmap, u32 index){
    u8 *byte;
    u8 mask;
    
    byte = bitmap + index / BITS_PER_BYTE;
    mask = 1 << (index % BITS_PER_BYTE);
    *byte = mask | (*byte);
}

void reset_bit(u8 *bitmap, u32 index){
    u8 *byte;
    u8 mask;
    
    byte = bitmap + index / BITS_PER_BYTE;
    mask = 1 << (index % BITS_PER_BYTE);
    mask = mask ^ 0xFF;
    *byte = mask & (*byte);
}
