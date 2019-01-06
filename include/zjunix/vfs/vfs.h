#ifndef _ZJUNIX_VFS_VFS_H
#define _ZJUNIX_VFS_VFS_H

#include <zjunix/type.h>
#include <zjunix/list.h>
#include <zjunix/vfs/err.h>

#define DPT_MAX_ENTRY_COUNT                     4
#define DPT_ENTRY_LEN                           16
#define SECTOR_SIZE                             512
#define SECTOR_SHIFT                            9
#define BITS_PER_BYTE                           8

#define S_CLEAR                                 0
#define S_DIRTY                                 1

#define MAX_FILE_NAME_LEN                       255

#define D_PINNED                                1
#define D_UNPINNED                              0

#define O_RDONLY	                            0x0000                 
#define O_WRONLY	                            0x0001                  
#define O_RDWR		                            0x0002                
#define O_ACCMODE	                            0x0003
#define O_CREAT		                            0x0100             
#define O_APPEND	                            0x2000                  
#define ACC_MODE(x) ("/000/004/002/006"[(x)&O_ACCMODE])

#define MAY_APPEND	                            0x0008

#define LOOKUP_FOLLOW                           0x0001                
#define LOOKUP_DIRECTORY                        0x0002                  
#define LOOKUP_CONTINUE                         0x0004                
#define LOOKUP_PARENT                           0x0010              
#define LOOKUP_CREATE                           0x0200            

enum {LAST_NORM, LAST_ROOT, LAST_DOT, LAST_DOTDOT, LAST_BIND};

#define FMODE_READ		                        0x1                  
#define FMODE_WRITE		                        0x2                    
#define FMODE_LSEEK		                        0x4                    
#define FMODE_PREAD		                        0x8                     
#define FMODE_PWRITE	                        0x10                  

#define PAGE_SIZE                               (1 << PAGE_SHIFT)
#define PAGE_SHIFT                              12
#define PAGE_CACHE_SIZE                         PAGE_SIZE
#define PAGE_CACHE_SHIFT                        PAGE_SHIFT
#define PAGE_CACHE_MASK                         (~((1 << PAGE_SHIFT) - 1))

enum {
         FT_UNKNOWN,     
         FT_REG_FILE,    
         FT_DIR    
};

struct vfs_page;

struct master_boot_record {
    u32                                 m_count;                      
    u32                                 m_base[DPT_MAX_ENTRY_COUNT];    
    u8                                  m_data[SECTOR_SIZE];            
};

struct file_system_type {
    u8                                  *name;                  
};

struct super_block {
    u8                                  s_dirt;               
    u32                                 s_blksize;           
    struct file_system_type             *s_type;            
    struct dentry                       *s_root;                
    struct super_operations             *s_op;               
    void                                *s_fs_info;             
};

struct vfsmount {
	struct list_head                    mnt_hash;           
	struct vfsmount                     *mnt_parent;	        
	struct dentry                       *mnt_mountpoint;     
	struct dentry                       *mnt_root;             
	struct super_block                  *mnt_sb;            
};

struct address_space {
    u32                                 a_pagesize;            
    u32                                 *a_page;                
    struct inode                        *a_host;                
    struct list_head                    a_cache;             
    struct address_space_operations     *a_op;               
};

struct inode {
    u32                                 i_ino;                
    u32                                 i_count;            
    u32                                 i_blocks;          
    u32                                 i_blkbits;        
    u32                                 i_blksize;           
    u32                                 i_size;               
    struct list_head                    i_hash;                 
    struct list_head                    i_LRU;                 
    struct list_head                    i_dentry;              
    struct inode_operations             *i_op;                
    struct file_operations              *i_fop;              
    struct super_block                  *i_sb;               
    struct address_space                i_data;               
};

struct qstr {
    const u8                            *name;                
    u32                                 len;                    
    u32                                 hash;                
};

struct dentry {
    u32                                 d_count;            
    u32                                 d_pinned;             
    u32                                 d_mounted;              
    struct inode                        *d_inode;           
    struct list_head                    d_hash;              
    struct dentry                       *d_parent;            
    struct qstr                         d_name;            
    struct list_head                    d_LRU;               
    struct list_head                    d_child;             
    struct list_head                    d_subdirs;           
    struct list_head                    d_alias;           
    struct dentry_operations            *d_op;              
    struct super_block                  *d_sb;                
};

struct file {
    u32                                 f_pos;                 
	struct list_head	                f_list;                 
	struct dentry		                *f_dentry;              
	struct vfsmount                     *f_vfsmnt;              
	struct file_operations	            *f_op;                  
	u32 		                        f_flags;               
	u32			                        f_mode;                 
	struct address_space	            *f_mapping;       
};

struct open_intent {
    u32	                                flags;
	u32	                                create_mode;
};

struct nameidata {  
    struct dentry                       *dentry;                
    struct vfsmount                     *mnt;                  
    struct qstr                         last;                   
    u32                                 flags;              
    u32                                 last_type;             
    union {                                                  
		struct open_intent open;
	} intent;
};

struct path {
  	struct vfsmount                     *mnt;                   
  	struct dentry                       *dentry;             
};

struct dirent {
    u32                                 ino;                    
    u8                                  type;          
    const u8                            *name;             
};


struct getdent {
    u32                                 count;                 
    struct dirent                       *dirent;                
};

struct super_operations {
    u32 (*delete_inode) (struct dentry *);
    u32 (*write_inode) (struct inode *, struct dentry *);
};

struct file_operations {
    u32 (*read) (struct file *, u8 *, u32, u32 *);
    u32 (*write) (struct file *, u8 *, u32, u32 *);
    u32 (*flush) (struct file *);
    u32 (*readdir) (struct file *, struct getdent *);
};

struct inode_operations {
    u32 (*create) (struct inode *,struct dentry *, u32, struct nameidata *);
    struct dentry * (*lookup) (struct inode *,struct dentry *, struct nameidata *);
};

struct address_space_operations {
    u32 (*writepage)(struct vfs_page *)
    u32 (*readpage)(struct vfs_page *);
    u32 (*bmap)(struct inode *, u32);
};

struct dentry_operations {
    u32 (*compare)(const struct qstr *, const struct qstr *);
};

struct master_boot_record   * MBR;
struct dentry               * root_dentry;
struct dentry               * pwd_dentry;
struct vfsmount             * root_mnt;
struct vfsmount             * pwd_mnt;

// vfs.c
u32 init_vfs();
u32 vfs_read_MBR();

// open.c
struct file * vfs_open(const u8 *, u32, u32);
u32 open_namei(const u8 *, u32, u32, struct nameidata *);
u32 path_lookup(const u8 *, u32 , struct nameidata *);
u32 link_path_walk(const u8 *, struct nameidata *);
void follow_dotdot(struct vfsmount **, struct dentry **);
u32 do_lookup(struct nameidata *, struct qstr *, struct path *);
struct dentry * real_lookup(struct dentry *, struct qstr *, struct nameidata *);
struct dentry * __lookup_hash(struct qstr *, struct dentry *, struct nameidata *);
struct dentry * d_alloc(struct dentry *, const struct qstr *);
struct file * dentry_open(struct dentry *, struct vfsmount *, u32);
u32 vfs_close(struct file *);

// read_write.c
u32 vfs_read(struct file *file, char *buf, u32 count, u32 *pos);
u32 vfs_write(struct file *file, char *buf, u32 count, u32 *pos);
u32 generic_file_read(struct file *, u8 *, u32, u32 *);
u32 generic_file_write(struct file *, u8 *, u32, u32 *);
u32 generic_file_flush(struct file *);

// mount.c
u32 mount_ext2();
u32 follow_mount(struct vfsmount **, struct dentry **);
struct vfsmount * lookup_mnt(struct vfsmount *, struct dentry *);

// utils.c
u16 get_u16(u8 *);
u32 get_u32(u8 *);
void set_u16(u8 *, u16);
void set_u32(u8 *, u32);
u32 read_block(u8 *, u32, u32);
u32 write_block(u8 *, u32, u32);
u32 generic_compare_filename(const struct qstr *, const struct qstr *);
u32 get_bit(const u8 *, u32);
void set_bit(u8 *, u32);
void reset_bit(u8 *, u32);

// usr.c
u32 vfs_cat(const u8 *);
u32 vfs_cd(const u8 *);
u32 vfs_ls(const u8 *);
u32 vfs_rm(const u8 *);



#endif
