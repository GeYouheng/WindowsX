#ifndef _ZJUNIX_VFS_EXT_2_H
#define _ZJUNIX_VFS_EXT_2_H

#include <zjunix/vfs/vfs.h>

#define                 EXT2_BOOT_BLOCK_SECT                2
#define                 EXT2_SUPER_BLOCK_SECT               2
#define                 EXT2_NAME_LEN                       255
#define                 EXT2_GROUP_DESC_BYTE                32
#define                 EXT2_ROOT_INO                       2
#define                 EXT2_N_BLOCKS                       15
#define                 EXT2_BASE_BLOCK_SIZE                1024
#define                 EXT2_FIRST_MAP_INDEX                12
#define                 EXT2_SECOND_MAP_INDEX               13
#define                 EXT2_THIRD_MAP_INDEX                14
#define                 EXT2_BLOCK_ADDR_SHIFT               2
#define                 MAX_DIRENT_NUM                      128

extern struct dentry                    * root_dentry;              // vfs.c
extern struct dentry                    * pwd_dentry;
extern struct vfsmount                  * root_mnt;
extern struct vfsmount                  * pwd_mnt;

extern struct cache                     * dcache;                   // vfscache.c
extern struct cache                     * pcache;
extern struct cache                     * icache;

extern struct vfs_page* tempp;

struct super_operations ext2_super_operations = {
    .delete_inode = ext2_delete_inode,
    .write_inode = ext2_write_inode,
};

struct inode_operations ext2_inode_operations[2] = {
    {
        .lookup = ext2_inode_lookup,
        .create = ext2_create,
    },
    {
        .create = ext2_create,
    }
};

struct dentry_operations ext2_dentry_operations = {
    .compare    = generic_compare_filename,
};

struct file_operations ext2_file_operations = {
    .read		= generic_file_read,
    .write      = generic_file_write,
    .flush      = generic_file_flush,
    .readdir    = ext2_readdir,
};

struct address_space_operations ext2_address_space_operations = {
    .writepage  = ext2_writepage,
    .readpage   = ext2_readpage,
    .bmap       = ext2_bmap,
};

enum {
         EXT2_FT_UNKNOWN,     
         EXT2_FT_REG_FILE,
         EXT2_FT_DIR,   
};

struct ext2_base_information {
    u32                 ex_base;                          
    u32                 ex_first_sb_sect;                
    u32                 ex_first_gdt_sect;                
    union {
        u8                  *data;
        struct ext2_super   *attr;
    } sb;                                              
};
        
struct ext2_super {
    u32                 inode_num;                          
    u32                 block_num;                    
    u32                 res_block_num;                   
    u32                 free_block_num;                  
    u32                 free_inode_num;                    
    u32                 first_data_block_no;           
    u32                 block_size;                      
    u32                 slice_size;                       
    u32                 blocks_per_group;               
    u32                 slices_per_group;                 
    u32                 inodes_per_group;                   
    u32                 install_time;                    
    u32                 last_write_in;                    
    u16                 install_count;                  
    u16                 max_install_count;                 
    u16                 magic;                          
    u16                 state;                            
    u16                 err_action;                      
    u16                 edition_change_mark;              
    u32                 last_check;                      
    u32                 max_check_interval;                 
    u32                 operating_system;              
    u32                 edition_mark;                       
    u16                 uid;                                
    u16                 gid;                          
    u32                 first_inode;                    
    u16                 inode_size;                       
};

struct ext2_dir_entry {
	u32	                ino;                             
	u16                 rec_len;                            
    u8	                name_len;                          
    u8                  file_type;                       
	char	            name[EXT2_NAME_LEN];                
};

struct ext2_group_desc {
	u32	                block_bitmap;                       
	u32	                inode_bitmap;                       
	u32	                inode_table;                        
	u16	                free_blocks_count;              
	u16	                free_inodes_count;                
	u16	                used_dirs_count;                
	u16	                pad;                             
	u32	                reserved[3];
};

struct ext2_inode {
	u16	                i_mode;                          
	u16	                i_uid;                            
	u32	                i_size;                             
	u32	                i_atime;                         
	u32	                i_ctime;                          
	u32	                i_mtime;                      
	u32	                i_dtime;                        
	u16	                i_gid;                            
	u16	                i_links_count;                    
	u32	                i_blocks;                        
	u32	                i_flags;                       
	u32                 osd1;                           
	u32	                i_block[EXT2_N_BLOCKS];             
	u32	                i_generation;                       
	u32	                i_file_acl;                       
	u32	                i_dir_acl;                         
    u32	                i_faddr;                          
    u32                 osd2[3];                        
};

u32 init_ext2(u32);
u32 ext2_delete_inode(struct dentry *);
u32 ext2_write_inode(struct inode *, struct dentry *);
struct dentry * ext2_inode_lookup(struct inode *, struct dentry *, struct nameidata *);
u32 ext2_create(struct inode *, struct dentry *, u32, struct nameidata *);
u32 ext2_readdir(struct file *, struct getdent *);
u32 ext2_readpage(struct vfs_page *);
u32 ext2_writepage(struct vfs_page *);
u32 ext2_bmap(struct inode *, u32);
u32 ext2_fill_inode(struct inode *);
u32 ext2_check_inode_bitmap(struct inode *);
u32 ext2_group_base_sect(struct inode *);


#endif
