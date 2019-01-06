#ifndef _ZJUNIX_VFS_FAT32_H
#define _ZJUNIX_VFS_FAT32_H

#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>

#include <zjunix/log.h>
#include <zjunix/slab.h>
#include <driver/vga.h>
#include <zjunix/utils.h>

#define MAX_FAT32_SHORT_FILE_NAME_BASE_LEN      8
#define MAX_FAT32_SHORT_FILE_NAME_EXT_LEN       3
#define MAX_FAT32_SHORT_FILE_NAME_LEN           ( MAX_FAT32_SHORT_FILE_NAME_BASE_LEN  + MAX_FAT32_SHORT_FILE_NAME_EXT_LEN )
#define FAT32_FAT_ENTRY_LEN_SHIFT               2
#define FAT32_DIR_ENTRY_LEN                     32

#define ATTR_DIRECTORY   0x10

#define FAT32_NAME_NORMAL_TO_SPECIFIC           0
#define FAT32_NAME_SPECIFIC_TO_NORMAL           1
#define INF                                     10000

extern struct dentry                    * root_dentry;              // vfs.c
extern struct dentry                    * pwd_dentry;
extern struct vfsmount                  * root_mnt;
extern struct vfsmount                  * pwd_mnt;

extern struct cache                     * dcache;                   // vfscache.c
extern struct cache                     * pcache;
extern struct cache                     * icache;

struct vfs_page * tempp;

struct fat32_basic_information {
    struct fat32_dos_boot_record* fa_DBR;               
    struct fat32_file_system_information* fa_FSINFO;    
    struct fat32_file_allocation_table* fa_FAT;        
};

struct fat32_dos_boot_record {
    u32 base;                                           
    u32 reserved;                                       
    u32 fat_num;                                    
    u32 fat_size;                                       
    u32 root_clu;                                       
    u32 sec_per_clu;                                 
    u8 data[SECTOR_SIZE];                           
};

struct fat32_file_system_information {
    u32 base;                                           
    u8 data[SECTOR_SIZE];                               
};

struct fat32_file_allocation_table {
    u32 base;                                           
    u32 data_sec;                                       
    u32 root_sec;                                    
};

struct __attribute__((__packed__)) fat_dir_entry {
    u8 name[MAX_FAT32_SHORT_FILE_NAME_LEN];             
    u8 attr;                                            
    u8 lcase;                                          
    u8 ctime_cs;                                      
    u16 ctime;                                      
    u16 cdate;                                         
    u16 adate;                                        
    u16 starthi;                                       
    u16 time;                                        
    u16 date;                                       
    u16 startlo;                                      
    u32 size;                                         
};

struct super_operations fat32_super_operations = {
    .delete_inode   = fat32_delete_inode,
    .write_inode    = fat32_write_inode,
};

struct inode_operations fat32_inode_operations[2] = {
    {
        .lookup = fat32_inode_lookup,
        .create = fat32_create,
    },
    {
        .create = fat32_create,
    }
};

struct dentry_operations fat32_dentry_operations = {
    .compare    = generic_compare_filename,
};

struct file_operations fat32_file_operations = {
    .read		= generic_file_read,
    .write      = generic_file_write,
    .flush      = generic_file_flush,
    .readdir    = fat32_readdir,
};

struct address_space_operations fat32_address_space_operations = {
    .writepage  = fat32_writepage,
    .readpage   = fat32_readpage,
    .bmap       = fat32_bmap,
};

u32 init_fat32(u32 base);
u32 fat32_delete_inode(struct dentry *dentry);
u32 fat32_write_inode(struct inode * inode, struct dentry * parent);
struct dentry* fat32_inode_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd);
u32 fat32_create(struct inode *dir, struct dentry *dentry, u32 mode, struct nameidata *nd);
u32 fat32_readdir(struct file * file, struct getdent * getdent);
void fat32_convert_filename(struct qstr* dest, const struct qstr* src, u8 mode, u32 direction);
u32 fat32_readpage(struct vfs_page *page);
u32 fat32_writepage(struct vfs_page *page);
u32 fat32_bmap(struct inode* inode, u32 pageNo);
u32 read_fat(struct inode* inode, u32 index);






















 
