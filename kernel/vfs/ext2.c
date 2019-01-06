#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>
#include <zjunix/vfs/ext2.h>
#include <zjunix/utils.h>
#include <zjunix/slab.h>
#include <zjunix/log.h>
#include <driver/vga.h>
#include <driver/ps2.h>
#include <driver/sd.h>

u32 init_ext2(u32 base){
    u32 i;
    u32 err;
    u32 p_location;
    struct vfs_page                     * curPage;
    struct ext2_base_information        * ext2_BI;
    struct file_system_type             * ext2_fs_type;
    struct super_block                  * ext2_sb;
    struct dentry                       * ext2_root_dentry;
    struct inode                        * ext2_root_inode;
    struct vfsmount                     * ext2_root_mnt;

    ext2_BI = (struct ext2_base_information *) kmalloc ( sizeof(struct ext2_base_information) );
    if (ext2_BI == 0)
        return -ENOMEM;
    ext2_BI->ex_base = base;
    ext2_BI->ex_first_sb_sect = ext2_BI->ex_base + EXT2_BOOT_BLOCK_SECT;
    
    ext2_BI->sb.data = (u8 *) kmalloc ( sizeof(u8) * EXT2_SUPER_BLOCK_SECT * SECTOR_SIZE );
    if (ext2_BI->sb.data == 0)
        return -ENOMEM;
    err = read_block(ext2_BI->sb.data, ext2_BI->ex_first_sb_sect, EXT2_SUPER_BLOCK_SECT);
    if (err)
        return -EIO;

    if (ext2_BI->sb.attr->block_size == 0 || ext2_BI->sb.attr->block_size == 1)
        ext2_BI->ex_first_gdt_sect = base + EXT2_BOOT_BLOCK_SECT + EXT2_SUPER_BLOCK_SECT;
    else
        ext2_BI->ex_first_gdt_sect = base + (( EXT2_BASE_BLOCK_SIZE << ext2_BI->sb.attr->block_size) >> SECTOR_SHIFT);
    
    ext2_fs_type = (struct file_system_type *) kmalloc ( sizeof(struct file_system_type) );
    if (ext2_fs_type == 0)
        return -ENOMEM;
    ext2_fs_type->name = "ext2";

    ext2_sb = (struct super_block *) kmalloc ( sizeof(struct super_block) );
    if (ext2_sb == 0)
        return -ENOMEM;
    ext2_sb->s_dirt    = S_CLEAR;
    ext2_sb->s_blksize = EXT2_BASE_BLOCK_SIZE << ext2_BI->sb.attr->block_size;
    ext2_sb->s_type    = ext2_fs_type;
    ext2_sb->s_root    = 0;
    ext2_sb->s_fs_info = (void*)ext2_BI;
    ext2_sb->s_op      = &ext2_super_operations;

    ext2_root_dentry = (struct dentry *) kmalloc ( sizeof(struct dentry) );
    if (ext2_root_dentry == 0)
        return -ENOMEM;
    ext2_root_dentry->d_count           = 1;
    ext2_root_dentry->d_mounted         = 0;
    ext2_root_dentry->d_inode           = 0;
    ext2_root_dentry->d_parent          = 0;
    ext2_root_dentry->d_name.name       = "/";
    ext2_root_dentry->d_name.len        = 1;
    ext2_root_dentry->d_sb              = ext2_sb;
    ext2_root_dentry->d_op              = &ext2_dentry_operations;
    INIT_LIST_HEAD(&(ext2_root_dentry->d_hash));
    INIT_LIST_HEAD(&(ext2_root_dentry->d_LRU));
    INIT_LIST_HEAD(&(ext2_root_dentry->d_subdirs));
    INIT_LIST_HEAD(&(ext2_root_dentry->d_child));
    INIT_LIST_HEAD(&(ext2_root_dentry->d_alias));
    dcache->c_op->add(dcache, (void*)ext2_root_dentry);

    ext2_sb->s_root = ext2_root_dentry;


    ext2_root_inode = (struct inode *) kmalloc ( sizeof(struct inode) );
    if (ext2_root_inode == 0)
        return -ENOMEM;
    ext2_root_inode->i_count            = 1;
    ext2_root_inode->i_ino              = EXT2_ROOT_INO;
    ext2_root_inode->i_op               = &(ext2_inode_operations[0]);
    ext2_root_inode->i_fop              = &ext2_file_operations;
    ext2_root_inode->i_sb               = ext2_sb;
    ext2_root_inode->i_blksize          = ext2_sb->s_blksize;
    INIT_LIST_HEAD(&(ext2_root_inode->i_hash));
    INIT_LIST_HEAD(&(ext2_root_inode->i_LRU));
    INIT_LIST_HEAD(&(ext2_root_inode->i_dentry));

    switch (ext2_root_inode->i_blksize ){
        case 1024: ext2_root_inode->i_blkbits = 10; break;
        case 2048: ext2_root_inode->i_blkbits = 11; break;
        case 4096: ext2_root_inode->i_blkbits = 12; break;
        case 8192: ext2_root_inode->i_blkbits = 13; break;
        default: return -EFAULT;
    }

    ext2_root_inode->i_data.a_host      = ext2_root_inode;
    ext2_root_inode->i_data.a_pagesize  = ext2_sb->s_blksize;
    ext2_root_inode->i_data.a_op        = &(ext2_address_space_operations);
    INIT_LIST_HEAD(&(ext2_root_inode->i_data.a_cache));

    err = ext2_fill_inode(ext2_root_inode);
    if (err)
        return err;

    for (i = 0; i < ext2_root_inode->i_blocks; i++){
        
        p_location = ext2_root_inode->i_data.a_op->bmap(ext2_root_inode, i);
        if (p_location == 0)
            continue;

        curPage = (struct vfs_page *) kmalloc ( sizeof(struct vfs_page) );
        if (curPage == 0)
            return -ENOMEM;

        curPage->p_state = P_CLEAR;
        curPage->p_location = p_location;
        curPage->p_mapping = &(ext2_root_inode->i_data);
        INIT_LIST_HEAD(&(curPage->p_hash));
        INIT_LIST_HEAD(&(curPage->p_LRU));
        INIT_LIST_HEAD(&(curPage->p_list));

        err = curPage->p_mapping->a_op->readpage(curPage);
        if ( IS_ERR_VALUE(err) ) {
            release_page(curPage);
            return err;
        }
        
        pcache->c_op->add(pcache, (void*)curPage);
        list_add(&(curPage->p_list), &(curPage->p_mapping->a_cache));
    }

    ext2_root_dentry->d_inode = ext2_root_inode;
    list_add(&(ext2_root_dentry->d_alias), &(ext2_root_inode->i_dentry));

    ext2_root_mnt = (struct vfsmount *) kmalloc ( sizeof(struct vfsmount));
    if (ext2_root_mnt == 0)
        return -ENOMEM;
    ext2_root_mnt->mnt_parent        = ext2_root_mnt;
    ext2_root_mnt->mnt_mountpoint    = ext2_root_dentry;
    ext2_root_mnt->mnt_root          = ext2_root_dentry;
    ext2_root_mnt->mnt_sb            = ext2_sb;
    INIT_LIST_HEAD(&(ext2_root_mnt->mnt_hash));

    list_add(&(ext2_root_mnt->mnt_hash), &(root_mnt->mnt_hash));

    return 0;
}

u32 ext2_delete_inode(struct dentry *dentry){
    u8 *data;
    u8 *start;
    u8 *end;
    u8 *copy_end;
    u8 *copy_start;
    u8 *paste_start;
    u8 *paste_end;
    u8 buffer[SECTOR_SIZE];
    u32 i;
    u32 len;
    u32 err;
    u32 flag;
    u32 sect;
    u32 found;
    u32 block;
    u32 group;
    u32 rec_len;
    u32 curPageNo;
    u32 group_base;       
    u32 blocks_per_group;
    u32 inodes_per_group;
    u32 sectors_per_block;
    u32 block_bitmap_base;           
    u32 inode_bitmap_base;
    u32 inode_table_base;
    struct inode                    *dir;
    struct inode                    *inode;
    struct inode                    *dummy;
    struct qstr                     qstr;
    struct condition                cond;
    struct address_space            *mapping;
    struct vfs_page                 *curPage;
    struct ext2_dir_entry           *ex_dir_entry;
    struct ext2_base_information    *ext2_BI;

    inode = dentry->d_inode;

    group_base                          = ext2_group_base_sect(inode);
    sectors_per_block                   = inode->i_blksize >> SECTOR_SHIFT;
    inode_bitmap_base                   = group_base + sectors_per_block;
    inode_table_base                    = group_base + (sectors_per_block << 1);

    ext2_BI             = (struct ext2_base_information *) inode->i_sb->s_fs_info;
    inodes_per_group    = ext2_BI->sb.attr->inodes_per_group;
    blocks_per_group    = ext2_BI->sb.attr->blocks_per_group;

    dummy = (struct inode*) kmalloc(sizeof(struct inode));          
    dummy->i_blksize    = inode->i_blksize;
    dummy->i_sb         = inode->i_sb;

    for ( i = 0; i < inode->i_blocks; i++ ){
        block = ext2_bmap(inode, i);
        if  (block == 0)
            continue;

        group = block / blocks_per_group;
        dummy->i_ino = group * inodes_per_group + 1;
        block_bitmap_base = ext2_group_base_sect(dummy);

        sect = block_bitmap_base + ( block % blocks_per_group) / SECTOR_SIZE / BITS_PER_BYTE;

        err = read_block(buffer, sect, 1);
        if (err)
            return -EIO;

        reset_bit(buffer, (block % blocks_per_group) % (SECTOR_SIZE * BITS_PER_BYTE));

        err = write_block(buffer, sect, 1);
        if (err)
            return -EIO;

        err = read_block(buffer, sect, 1);
        if (err)
            return -EIO;

    }

    sect                = inode_bitmap_base + ((inode->i_ino - 1) % inodes_per_group) / SECTOR_SIZE / BITS_PER_BYTE;
    err = read_block(buffer, sect, 1);
    if (err)
        return -EIO;

    reset_bit(buffer, ((inode->i_ino - 1) % inodes_per_group) % (SECTOR_SIZE * BITS_PER_BYTE));

    err = write_block(buffer, sect, 1);
    if (err)
        return -EIO;

    err = read_block(buffer, sect, 1);
    if (err)
        return -EIO;
    

    sect = inode_table_base + (inode->i_ino - 1) % inodes_per_group / ( SECTOR_SIZE / ext2_BI->sb.attr->inode_size);

    err = read_block(buffer, sect, 1);
    if (err)
        return -EIO;
    
    kernel_memset(buffer + ((inode->i_ino - 1) % inodes_per_group % ( SECTOR_SIZE / ext2_BI->sb.attr->inode_size)) * ext2_BI->sb.attr->inode_size, \
                    0, ext2_BI->sb.attr->inode_size);
    
    err = write_block(buffer, sect, 1);
    if (err)
        return -EIO;

    flag = 0;
    found = 0;
    rec_len = 0;
    copy_start = 0;
    dir = dentry->d_parent->d_inode;
    mapping = &(dir->i_data);
    for ( i = 0; i < dir->i_blocks; i++){     
        curPageNo = mapping->a_op->bmap(dir, i);
        if (curPageNo == 0)
            return -ENOENT;

        cond.cond1 = (void*)(&curPageNo);
        cond.cond2 = (void*)(dir);
        curPage = pcache->c_op->look_up(pcache, &cond);

        if ( curPage == 0 ){
            curPage = (struct vfs_page *) kmalloc ( sizeof(struct vfs_page) );
            if (!curPage)
                return -ENOMEM;

            curPage->p_state    = P_CLEAR;
            curPage->p_location = curPageNo;
            curPage->p_mapping  = mapping;
            INIT_LIST_HEAD(&(curPage->p_hash));
            INIT_LIST_HEAD(&(curPage->p_LRU));
            INIT_LIST_HEAD(&(curPage->p_list));

            err = mapping->a_op->readpage(curPage);
            if ( IS_ERR_VALUE(err) ){
                release_page(curPage);
                return 0;
            }

            curPage->p_state = P_CLEAR;
            pcache->c_op->add(pcache, (void*)curPage);
            list_add(&(curPage->p_list), &(mapping->a_cache));
        }

        data = curPage->p_data;
        start = data;
        end = data + inode->i_blksize;
        while ( *data != 0 && data != end) {
            ex_dir_entry = (struct ext2_dir_entry *)data;

            if (found){                                  
                if (flag == 0) {          
                    copy_start = data;
                    flag = 1;
                }
                copy_end = data + ex_dir_entry->rec_len;
            }
            else {
                qstr.len = ex_dir_entry->name_len;
                qstr.name = ex_dir_entry->name;
                if ( generic_compare_filename( &qstr, &(dentry->d_name) ) == 0 ){ 
                    paste_start = data;
                    found = 1;
                    rec_len = ex_dir_entry->rec_len;
                }
            }  
            data += (ex_dir_entry->rec_len);
        }

        if (found)
            break;                            
    }

    if (!found)
        return -ENOENT;

    for ( i = 0; i < rec_len; i++ )
        *(paste_start+i) = 0;

    if (copy_start != 0) {
        len = (u32)copy_end - (u32)copy_start;            
        kernel_memcpy(paste_start, copy_start, len);

        paste_end = paste_start + len;
        len = (u32)copy_end - (u32)paste_end;          
        for ( i = 0; i < len; i++ )
            *(paste_end+i) = 0;

        err = ext2_writepage(curPage);                   
        if (err)
            return err;
    }

    return 0;
}

u32 ext2_write_inode(struct inode * inode, struct dentry * parent){
    u8 buffer[SECTOR_SIZE];
    u32 i;
    u32 err;
    u32 sect;
    u32 group_sect_base;
    u32 inodes_per_group;
    struct ext2_inode               * ex_inode;
    struct ext2_base_information    * ext2_BI;

    group_sect_base = ext2_group_base_sect(inode);
    if (group_sect_base == 0)
        return -EIO;

    ext2_BI             = (struct ext2_base_information *) inode->i_sb->s_fs_info;
    inodes_per_group    = ext2_BI->sb.attr->inodes_per_group;
    sect                = group_sect_base + 2 * (inode->i_blksize >> SECTOR_SHIFT) + \
                            ( (inode->i_ino - 1) % inodes_per_group ) / ( SECTOR_SIZE / ext2_BI->sb.attr->inode_size);
    err = read_block(buffer, sect, 1);
    if (err)
        return -EIO;
    
    ex_inode            = (struct ext2_inode *)(buffer + \
        (inode->i_ino - 1) % ( SECTOR_SIZE / ext2_BI->sb.attr->inode_size) * ext2_BI->sb.attr->inode_size );

    ex_inode->i_size                   = inode->i_size;

    err = write_block(buffer, sect, 1);
    if (err)
        return -EIO;

    return 0;
}

struct dentry * ext2_inode_lookup(struct inode * dir, struct dentry * dentry, struct nameidata * nd) {
    u8 *data;
    u8 *end;
    u32 i;
    u32 err;
    u32 addr;
    u32 found;
    u32 curPageNo;
    struct condition                        cond;
    struct qstr                             qstr;
    struct vfs_page                         * curPage;
    struct address_space                    * mapping;
    struct inode                            * new_inode;
    struct ext2_dir_entry                   * ex_dir_entry;

    found = 0;
    new_inode = 0;
    mapping = &(dir->i_data);

    for ( i = 0; i < dir->i_blocks; i++){
        curPageNo = mapping->a_op->bmap(dir, i);
        if (curPageNo == 0)
            return ERR_PTR(-ENOENT);

        cond.cond1 = (void*)(&curPageNo);
        cond.cond2 = (void*)(dir);
        curPage = (struct vfs_page *) pcache->c_op->look_up(pcache, &cond);

        if ( curPage == 0 ){
            curPage = (struct vfs_page *) kmalloc ( sizeof(struct vfs_page) );
            if (!curPage)
                return ERR_PTR(-ENOMEM);

            curPage->p_state    = P_CLEAR;
            curPage->p_location = curPageNo;
            curPage->p_mapping  = mapping;
            INIT_LIST_HEAD(&(curPage->p_hash));
            INIT_LIST_HEAD(&(curPage->p_LRU));
            INIT_LIST_HEAD(&(curPage->p_list));

            err = mapping->a_op->readpage(curPage);
            if ( IS_ERR_VALUE(err) ){
                release_page(curPage);
                return 0;
            }

            curPage->p_state = P_CLEAR;
            pcache->c_op->add(pcache, (void*)curPage);
            list_add(&(curPage->p_list), &(mapping->a_cache));
        }

        data = curPage->p_data;
        end = data + dir->i_blksize;
        while ( *data != 0 && data != end ) {
            ex_dir_entry = (struct ext2_dir_entry *)data;
            qstr.len = ex_dir_entry->name_len;
            qstr.name = ex_dir_entry->name;

            if ( generic_compare_filename( &qstr, &(dentry->d_name) ) == 0 ){
                new_inode = (struct inode*) kmalloc ( sizeof(struct inode) );
                new_inode->i_ino            = ex_dir_entry->ino;
                new_inode->i_blksize        = dir->i_blksize;
                new_inode->i_sb             = dir->i_sb;

                if ( 0 == ext2_check_inode_bitmap(new_inode)){
                    kfree(new_inode);
                    data += (ex_dir_entry->rec_len);
                    continue;
                }

                new_inode->i_count          = 1;
                new_inode->i_blkbits        = dir->i_blkbits;
                new_inode->i_fop            = &(ext2_file_operations);
                INIT_LIST_HEAD(&(new_inode->i_dentry));

                ext2_fill_inode(new_inode);

                if ( ex_dir_entry->file_type == EXT2_FT_DIR )                    
                    new_inode->i_op         = &(ext2_inode_operations[0]);
                else
                    new_inode->i_op         = &(ext2_inode_operations[1]);

                new_inode->i_data.a_host        = new_inode;
                new_inode->i_data.a_pagesize    = new_inode->i_blksize;
                new_inode->i_data.a_op          = &(ext2_address_space_operations);
                INIT_LIST_HEAD(&(new_inode->i_data.a_cache));

                // icache->c_op->add(icache, (void*)new_inode);
                found = 1;
                break;
            }
            data += (ex_dir_entry->rec_len);
        }
        if (found)
            break;                             
    }

    if (!found)
        return 0;

    dentry->d_inode = new_inode;
    dentry->d_op = &ext2_dentry_operations;
    list_add(&dentry->d_alias, &new_inode->i_dentry);
    
    
    return dentry;
}

u32 ext2_create(struct inode *dir, struct dentry *dentry, u32 mode, struct nameidata *nd) {
    return 0;
};

u32 ext2_readdir(struct file * file, struct getdent * getdent){
    u8 *data;
    u8 *name;
    u8 *end;
    u32 i;
    u32 j;
    u32 err;
    u32 pagesize;
    u32 curPageNo;
    struct inode                    *dir;
    struct inode                    *new_inode;
    struct qstr                     qstr;
    struct condition                cond;
    struct vfs_page                 *curPage;
    struct address_space            *mapping;
    struct ext2_dir_entry           *ex_dir_entry;

    dir = file->f_dentry->d_inode;
    mapping = &(dir->i_data);
    pagesize = dir->i_blksize;
    getdent->count = 0;
    getdent->dirent = (struct dirent *) kmalloc ( sizeof(struct dirent) * (MAX_DIRENT_NUM));
    if (getdent->dirent == 0)
        return -ENOMEM;

    for ( i = 0; i < dir->i_blocks; i++){
        curPageNo = mapping->a_op->bmap(dir, i);
        if (curPageNo == 0)
            continue;

        cond.cond1 = (void*)(&curPageNo);
        cond.cond2 = (void*)(dir);
        curPage = (struct vfs_page *) pcache->c_op->look_up(pcache, &cond);

        if ( curPage == 0 ){
            curPage = (struct vfs_page *) kmalloc ( sizeof(struct vfs_page) );
            if (!curPage)
                return -ENOMEM;

            curPage->p_state    = P_CLEAR;
            curPage->p_location = curPageNo;
            curPage->p_mapping  = mapping;
            INIT_LIST_HEAD(&(curPage->p_hash));
            INIT_LIST_HEAD(&(curPage->p_LRU));
            INIT_LIST_HEAD(&(curPage->p_list));

            err = mapping->a_op->readpage(curPage);
            if ( IS_ERR_VALUE(err) ){
                release_page(curPage);
                return 0;
            }

            curPage->p_state = P_CLEAR;
            pcache->c_op->add(pcache, (void*)curPage);
            list_add(&(curPage->p_list), &(mapping->a_cache));
        }

        data = curPage->p_data;
        end = data + dir->i_blksize;
        while ( *data != 0 && data != end ) {
            ex_dir_entry = (struct ext2_dir_entry *)data;
            new_inode = (struct inode*) kmalloc ( sizeof(struct inode) );
            new_inode->i_ino            = ex_dir_entry->ino;
            new_inode->i_blksize        = dir->i_blksize;
            new_inode->i_sb             = dir->i_sb;

            if ( 0 == ext2_check_inode_bitmap(new_inode)){
                kfree(new_inode);
                data += (ex_dir_entry->rec_len);
                continue;
            }

            qstr.len    = ex_dir_entry->name_len;
            qstr.name   = ex_dir_entry->name;

            name = 0;
            name = (u8 *) kmalloc ( sizeof(u8) * ( ex_dir_entry->name_len + 1 ));
            if (name == 0)
                return -ENOMEM;
            for ( j = 0; j < ex_dir_entry->name_len; j++)
                name[j] = qstr.name[j];
            name[j] = 0;

            getdent->dirent[getdent->count].ino     = ex_dir_entry->ino;
            getdent->dirent[getdent->count].name    = name;
            getdent->dirent[getdent->count].type    = ex_dir_entry->file_type;
            getdent->count += 1;
            
            data += (ex_dir_entry->rec_len);
        }  
    }   

    return 0;
}

u32 ext2_readpage(struct vfs_page * page) {
    u32 err;
    u32 base;
    u32 abs_sect_addr;
    struct inode *inode;

    inode = page->p_mapping->a_host;
    base = ((struct ext2_base_information *)(inode->i_sb->s_fs_info))->ex_base;
    abs_sect_addr = base + page->p_location * (inode->i_blksize >> SECTOR_SHIFT);

    page->p_data = ( u8* ) kmalloc ( sizeof(u8) * inode->i_blksize );
    if (page->p_data == 0)
        return -ENOMEM;

    kernel_memset(page->p_data, 0, sizeof(u8) * inode->i_blksize);
    err = read_block(page->p_data, abs_sect_addr, inode->i_blksize >> SECTOR_SHIFT);
    if (err)
        return -EIO;

    return 0;
}

u32 ext2_writepage(struct vfs_page * page) {
    u32 err;
    u32 base;
    u32 abs_sect_addr;
    struct inode *inode;

    inode = page->p_mapping->a_host;
    base = ((struct ext2_base_information *)(inode->i_sb->s_fs_info))->ex_base;
    abs_sect_addr = base + page->p_location * (inode->i_blksize >> SECTOR_SHIFT);
    
    err = write_block(page->p_data, abs_sect_addr, inode->i_blksize >> SECTOR_SHIFT);
    if (err)
        return -EIO;
    
    return 0;
}

u32 ext2_bmap(struct inode * inode, u32 curPageNo) {
    u8* data;
    u32 i;
    u32 addr;
    u32 *page;
    u32 ret_val;
    u32 first_no;
    u32 entry_num;
    page = inode->i_data.a_page;
  
    if ( curPageNo < EXT2_FIRST_MAP_INDEX ) 
        ret_val = page[curPageNo];  
  
    entry_num = inode->i_blksize >> EXT2_BLOCK_ADDR_SHIFT;         
    data = (u8 *) kmalloc ( inode->i_blksize * sizeof(u8) );
    if (data == 0)
        return 0;                                                 

    curPageNo -= EXT2_FIRST_MAP_INDEX;
    if ( curPageNo < entry_num ) {  
        read_block(data, page[EXT2_FIRST_MAP_INDEX], inode->i_blksize >> SECTOR_SHIFT);
        ret_val = get_u32(data + (curPageNo << EXT2_BLOCK_ADDR_SHIFT));
        goto ok;
    }  
  
    curPageNo -= entry_num;  
    if ( curPageNo < entry_num * entry_num ){
        read_block(data, page[EXT2_SECOND_MAP_INDEX], inode->i_blksize >> SECTOR_SHIFT);
        addr = get_u32(data + ((curPageNo / entry_num) << EXT2_BLOCK_ADDR_SHIFT) );
        read_block(data, addr, inode->i_blksize >> SECTOR_SHIFT);
        ret_val = get_u32(data + ((curPageNo % entry_num) << EXT2_BLOCK_ADDR_SHIFT) );
        goto ok;
    }

    curPageNo -= entry_num * entry_num; 
    if ( curPageNo < entry_num * entry_num * entry_num ){
        read_block(data, page[EXT2_THIRD_MAP_INDEX], inode->i_blksize >> SECTOR_SHIFT);
        addr = get_u32(data + ((curPageNo / entry_num / entry_num) << EXT2_BLOCK_ADDR_SHIFT) );
        read_block(data, addr, inode->i_blksize >> SECTOR_SHIFT);
        addr = get_u32(data + ((curPageNo % (entry_num / entry_num)) << EXT2_BLOCK_ADDR_SHIFT) );
        read_block(data, addr, inode->i_blksize >> SECTOR_SHIFT);
        ret_val = get_u32(data + ((curPageNo % entry_num % entry_num) << EXT2_BLOCK_ADDR_SHIFT) );
        goto ok;
    }

ok:
    kfree(data);
    return ret_val;
};

u32 ext2_fill_inode(struct inode *inode) {
    u8 buffer[SECTOR_SIZE];
    u32 i;
    u32 err;
    u32 sect;
    u32 group_sect_base;
    u32 inodes_per_group;
    struct ext2_inode               * ex_inode;
    struct ext2_base_information    * ext2_BI;

    group_sect_base = ext2_group_base_sect(inode);
    if (group_sect_base == 0)
        return -EIO;

    ext2_BI             = (struct ext2_base_information *) inode->i_sb->s_fs_info;
    inodes_per_group    = ext2_BI->sb.attr->inodes_per_group;
    sect                = group_sect_base + 2 * (inode->i_blksize >> SECTOR_SHIFT) + \
                            ( (inode->i_ino - 1) % inodes_per_group ) / ( SECTOR_SIZE / ext2_BI->sb.attr->inode_size);
    err = read_block(buffer, sect, 1);
    if (err)
        return -EIO;
    
    ex_inode            = (struct ext2_inode *)(buffer + \
        (inode->i_ino - 1) % ( SECTOR_SIZE / ext2_BI->sb.attr->inode_size) * ext2_BI->sb.attr->inode_size );

    inode->i_blocks                     = ex_inode->i_blocks;
    inode->i_size                       = ex_inode->i_size;

    inode->i_data.a_page = (u32 *) kmalloc (EXT2_N_BLOCKS * sizeof(u32));
    if (inode->i_data.a_page == 0)
        return -ENOMEM;
    for ( i = 0; i < EXT2_N_BLOCKS; i++ )
        inode->i_data.a_page[i] = ex_inode->i_block[i];

    return 0;
}

u32 ext2_check_inode_bitmap(struct inode *inode){
    u8 buffer[SECTOR_SIZE];
    u32 err;
    u32 sect;
    u32 state;
    u32 group_sect_base;
    u32 inodes_per_group;
    struct ext2_base_information * ext2_BI;

    group_sect_base = ext2_group_base_sect(inode);
    if (group_sect_base == 0)
        return 0;

    ext2_BI             = (struct ext2_base_information *) inode->i_sb->s_fs_info;
    inodes_per_group    = ext2_BI->sb.attr->inodes_per_group;
    sect                = group_sect_base + 1 * (inode->i_blksize >> SECTOR_SHIFT) + \
                            ( (inode->i_ino - 1) % inodes_per_group) / SECTOR_SIZE / BITS_PER_BYTE;

    err = read_block(buffer, sect, 1);
    if (err)
        return 0;

    state = get_bit(buffer, (inode->i_ino - 1 ) % inodes_per_group % (SECTOR_SIZE * BITS_PER_BYTE));
    return state;
}

u32 ext2_group_base_sect(struct inode * inode){
    u8 buffer[SECTOR_SIZE];
    u32 err;
    u32 base;
    u32 sect;
    u32 group;
    u32 offset;
    u32 blksize;
    u32 group_sect_base;
    u32 group_block_base;
    u32 inodes_per_group;
    struct ext2_base_information    * ext2_BI;

    ext2_BI             = (struct ext2_base_information *) inode->i_sb->s_fs_info;
    base                = ext2_BI->ex_base;
    blksize             = inode->i_blksize;
    inodes_per_group    = ext2_BI->sb.attr->inodes_per_group;
    group               = (inode->i_ino - 1) / inodes_per_group;            
    sect                = ext2_BI->ex_first_gdt_sect + group / (SECTOR_SIZE / EXT2_GROUP_DESC_BYTE);
    offset              = group % (SECTOR_SIZE / EXT2_GROUP_DESC_BYTE) * EXT2_GROUP_DESC_BYTE;

    err = read_block(buffer, sect, 1);
    if (err)
        return 0;
    group_block_base    = get_u32(buffer + offset);
    group_sect_base     = base + group_block_base * (inode->i_blksize >> SECTOR_SHIFT);

    return group_sect_base;
}

