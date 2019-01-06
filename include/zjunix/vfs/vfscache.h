#ifndef _ZJUNIX_VFS_VFSCACHE_H
#define _ZJUNIX_VFS_VFSCACHE_H

#include <zjunix/vfs/vfs.h>

#define DCACHE_CAPACITY                 16
#define DCACHE_HASHTABLE_SIZE          16

#define ICACHE_CAPACITY                 16
#define ICACHE_HASHTABLE_SIZE          16

#define PCACHE_CAPACITY                 64
#define PCACHE_HASHTABLE_SIZE           16

#define P_CLEAR                         0
#define P_DIRTY                         1

struct vfs_page {
    u8                          *p_data;                    
    u32                         p_state;                   
    u32                         p_location;             
    struct list_head            p_hash;                
    struct list_head            p_LRU;                    
    struct list_head            p_list;                    
    struct address_space        *p_mapping;              
};


struct cache {
    u32                         c_size;                 
    u32                         c_capacity;                
    u32                         c_tablesize;            
    struct list_head            c_LRU;                      
    struct list_head            *c_hashtable;              
    struct cache_operations     *c_op;                      
};


struct condition {
    void    *cond1;
    void    *cond2;
    void    *cond3;
};


struct cache_operations {
    void* (*look_up)(struct cache*, struct condition*);
    void (*add)(struct cache*, void*);
    u32 (*is_full)(struct cache*);
    void (*write_back)(void*);
};

// vfscache.c
u32 init_cache();
void cache_init(struct cache *, u32, u32);
u32 cache_is_full(struct cache *);

void* dcache_look_up(struct cache *, struct condition *);
void dcache_add(struct cache *, void *);
void dcache_put_LRU(struct cache *);

// void* icache_look_up(struct cache *, struct condition *);
// void icache_add(struct cache *, void *);
// void icache_put_LRU(struct cache *);
// void icache_write_back(void *);

void* pcache_look_up(struct cache *, struct condition *);
void pcache_add(struct cache *, void *);
void pcache_put_LRU(struct cache *);
void pcache_write_back(void *);

void dget(struct dentry *);
void dput(struct dentry *);

void release_dentry(struct dentry *);
void release_inode(struct inode *);
void release_page(struct vfs_page *);

u32 __intHash(u32, u32);
u32 __stringHash(struct qstr *, u32);

// utils.c
u32 read_block(u8 *, u32, u32);
u32 write_block(u8 *, u32, u32);

#endif
