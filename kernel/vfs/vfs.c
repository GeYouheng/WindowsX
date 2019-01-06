#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>
#include <zjunix/vfs/fat32.h>
#include <zjunix/vfs/ext2.h>

#include <zjunix/log.h>
#include <zjunix/slab.h>
#include <zjunix/utils.h>
#include <driver/vga.h>
 

u32 init_vfs(){
    u32 err;

    err = vfs_read_MBR();                      
    if ( IS_ERR_VALUE(err) ){
        log(LOG_FAIL, "vfs_read_MBR()");
        goto vfs_init_err;
    }
    log(LOG_OK, "vfs_read_MBR()");
    // while(1);
    err = init_cache();                         
    if ( IS_ERR_VALUE(err) ){
        log(LOG_FAIL, "init_cache()");
        goto vfs_init_err;
    }
    log(LOG_OK, "init_cache()");

    err = init_fat32(MBR->m_base[0]);      
    if ( IS_ERR_VALUE(err) ){
        log(LOG_FAIL, "init_fat32()");
        goto vfs_init_err;
    }
    log(LOG_OK, "init_fat32()");

    err = init_ext2(MBR->m_base[1]);           
    if ( IS_ERR_VALUE(err) ){
        log(LOG_FAIL, "init_ext2()");
        goto vfs_init_err;
    }
    log(LOG_OK, "init_ext2()");

    err = mount_ext2();                   
    if ( IS_ERR_VALUE(err) ){
        log(LOG_FAIL, "mount_ext2()");
        goto vfs_init_err;
    }
    log(LOG_OK, "mount_ext2()");

    return 0;

vfs_init_err:
    kernel_printf("vfs_init_err: %d\n", (int)(-err));  
    return err;
}

u32 vfs_read_MBR(){
    u8  *DPT_cur;
    u32 err;
    u32 part_base;

    MBR = (struct master_boot_record*) kmalloc( sizeof(struct master_boot_record) );
    if (MBR == 0)
        return -ENOMEM;
    
    kernel_memset(MBR->m_data, 0, sizeof(MBR->m_data));
    if ( err = read_block(MBR->m_data, 0, 1) )      
        goto vfs_read_MBR_err;

    MBR->m_count = 0;
    DPT_cur = MBR->m_data + 446 + 8;
    while ((part_base = get_u32(DPT_cur)) && MBR->m_count != DPT_MAX_ENTRY_COUNT ) {
        MBR->m_base[MBR->m_count++] = part_base;
        DPT_cur += DPT_ENTRY_LEN;
    }
    return 0;

vfs_read_MBR_err:
    kfree(MBR);
    return -EIO;
}



