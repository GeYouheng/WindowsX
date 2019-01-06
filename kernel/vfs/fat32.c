#include <zjunix/vfs/fat32.h>

u32 init_fat32(u32 base){
    u32 i;
    u32 j;
    u32 k;
    u32 next_clu;
    u32 temp;
    u32 err;
    struct fat32_basic_information  * fat32_BI;
    struct file_system_type         * fat32_fs_type;
    struct super_block              * fat32_sb;
    struct vfs_page                 * curPage;

    fat32_BI = (struct fat32_basic_information *) kmalloc ( sizeof(struct fat32_basic_information) );
    if (fat32_BI == 0)
        return -ENOMEM;
    fat32_BI->fa_DBR    = 0;
    fat32_BI->fa_FSINFO = 0;
    fat32_BI->fa_FAT    = 0;

    fat32_BI->fa_DBR = (struct fat32_dos_boot_record *) kmalloc ( sizeof(struct fat32_dos_boot_record) );
    if (fat32_BI->fa_DBR == 0)
        return -ENOMEM;
    fat32_BI->fa_DBR->base = base;
    kernel_memset(fat32_BI->fa_DBR->data, 0, sizeof(fat32_BI->fa_DBR->data));
    err = read_block(fat32_BI->fa_DBR->data, fat32_BI->fa_DBR->base, 1);        
    if (err)
        return -EIO;

    fat32_BI->fa_DBR->sec_per_clu   = *(fat32_BI->fa_DBR->data + 0x0D);
    fat32_BI->fa_DBR->reserved      = get_u16 (fat32_BI->fa_DBR->data + 0x0E);
    fat32_BI->fa_DBR->fat_num       = *(fat32_BI->fa_DBR->data + 0x10);
    fat32_BI->fa_DBR->fat_size      = get_u32 (fat32_BI->fa_DBR->data + 0x24);
    fat32_BI->fa_DBR->root_clu      = get_u32 (fat32_BI->fa_DBR->data + 0x2C);

    fat32_BI->fa_FSINFO = (struct fat32_file_system_information *) kmalloc \
        ( sizeof(struct fat32_file_system_information) );
    if (fat32_BI->fa_FSINFO == 0)
        return -ENOMEM;
    fat32_BI->fa_FSINFO->base = fat32_BI->fa_DBR->base + 1;                 
    kernel_memset(fat32_BI->fa_FSINFO->data, 0, sizeof(fat32_BI->fa_FSINFO->data));
    err = read_block(fat32_BI->fa_FSINFO->data, fat32_BI->fa_FSINFO->base, 1);
    if (err)
        return -EIO;

    fat32_BI->fa_FAT = (struct fat32_file_allocation_table *) kmalloc \
        ( sizeof(struct fat32_file_allocation_table) );
    if (fat32_BI->fa_FAT == 0)
        return -ENOMEM;
    fat32_BI->fa_FAT->base = base + fat32_BI->fa_DBR->reserved;                 

    fat32_BI->fa_FAT->data_sec = fat32_BI->fa_FAT->base + fat32_BI->fa_DBR->fat_num * \
        fat32_BI->fa_DBR->fat_size;
    fat32_BI->fa_FAT->root_sec = fat32_BI->fa_FAT->data_sec + \
        ( fat32_BI->fa_DBR->root_clu - 2 ) * fat32_BI->fa_DBR->sec_per_clu;
                                                                                

    fat32_fs_type = (struct file_system_type *) kmalloc ( sizeof(struct file_system_type) );
    if (fat32_fs_type == 0)
        return -ENOMEM;
    fat32_fs_type->name = "fat32";

    fat32_sb = (struct super_block *) kmalloc ( sizeof(struct super_block) );
    if (fat32_sb == 0)
        return -ENOMEM;
    fat32_sb->s_dirt    = S_CLEAR;
    fat32_sb->s_blksize = fat32_BI->fa_DBR->sec_per_clu << SECTOR_SHIFT;
    fat32_sb->s_type    = fat32_fs_type;
    fat32_sb->s_root    = 0;
    fat32_sb->s_fs_info = (void*)fat32_BI;
    fat32_sb->s_op      = &fat32_super_operations;

    root_dentry = (struct dentry *) kmalloc ( sizeof(struct dentry) );
    if (root_dentry == 0)
        return -ENOMEM;
    root_dentry->d_count        = 1;
    root_dentry->d_mounted      = 0;
    root_dentry->d_inode        = 0;
    root_dentry->d_parent       = 0;
    root_dentry->d_name.name    = "/";
    root_dentry->d_name.len     = 1;
    root_dentry->d_sb           = fat32_sb;
    root_dentry->d_op           = &fat32_dentry_operations;
    INIT_LIST_HEAD(&(root_dentry->d_hash));
    INIT_LIST_HEAD(&(root_dentry->d_LRU));
    INIT_LIST_HEAD(&(root_dentry->d_subdirs));
    INIT_LIST_HEAD(&(root_dentry->d_child));
    INIT_LIST_HEAD(&(root_dentry->d_alias));
    dcache->c_op->add(dcache, (void*)root_dentry);

    pwd_dentry = root_dentry;
    fat32_sb->s_root = root_dentry;

    struct inode* root_inode;
    root_inode = (struct inode *) kmalloc ( sizeof(struct inode) );
    if (root_inode == 0)
        return -ENOMEM;
    root_inode->i_count             = 1;
    root_inode->i_ino               = fat32_BI->fa_DBR->root_clu;
    root_inode->i_op                = &(fat32_inode_operations[0]);
    root_inode->i_fop               = &fat32_file_operations;
    root_inode->i_sb                = fat32_sb;
    root_inode->i_blocks            = 0;
    INIT_LIST_HEAD(&(root_inode->i_dentry));

    root_inode->i_blksize           = fat32_sb->s_blksize;


    switch (root_inode->i_blksize ){
        case 1024: root_inode->i_blkbits = 10; break;
        case 2048: root_inode->i_blkbits = 11; break;
        case 4096: root_inode->i_blkbits = 12; break;
        case 8192: root_inode->i_blkbits = 13; break;
        default: return -EFAULT;
    }
    INIT_LIST_HEAD(&(root_inode->i_hash));
    INIT_LIST_HEAD(&(root_inode->i_LRU));

    root_dentry->d_inode = root_inode;
    list_add(&(root_dentry->d_alias), &(root_inode->i_dentry));


    root_inode->i_data.a_host       = root_inode;
    root_inode->i_data.a_pagesize   = fat32_sb->s_blksize;
    root_inode->i_data.a_op         = &(fat32_address_space_operations);
    INIT_LIST_HEAD(&(root_inode->i_data.a_cache));
    
    i = 0;
    next_clu = fat32_BI->fa_DBR->root_clu;
    while ( 0x0FFFFFFF != next_clu ){
        root_inode->i_blocks ++;
        next_clu = read_fat(root_inode, next_clu);          
    }

    root_inode->i_data.a_page = (u32 *) kmalloc ( root_inode->i_blocks * sizeof(u32) );
    if (root_inode->i_data.a_page == 0)
            return -ENOMEM;
    kernel_memset(root_inode->i_data.a_page, 0, root_inode->i_blocks);

    next_clu = fat32_BI->fa_DBR->root_clu;
    for (i = 0; i < root_inode->i_blocks; i++){
        root_inode->i_data.a_page[i] = next_clu;
        next_clu = read_fat(root_inode, next_clu);
    }

    for (i = 0; i < root_inode->i_blocks; i++){
        curPage = (struct vfs_page *) kmalloc ( sizeof(struct vfs_page) );
        if (curPage == 0)
            return -ENOMEM;

        curPage->p_state = P_CLEAR;
        curPage->p_location = root_inode->i_data.a_page[i];
        curPage->p_mapping = &(root_inode->i_data);
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
        tempp = curPage;
    }

    root_mnt = (struct vfsmount *) kmalloc ( sizeof(struct vfsmount));
    if (root_mnt == 0)
        return -ENOMEM;
    root_mnt->mnt_parent        = root_mnt;
    root_mnt->mnt_mountpoint    = root_dentry;
    root_mnt->mnt_root          = root_dentry;
    root_mnt->mnt_sb            = fat32_sb;
    INIT_LIST_HEAD(&(root_mnt->mnt_hash));

    pwd_mnt = root_mnt;

    tempp = curPage;

    return 0;
}

u32 fat32_delete_inode(struct dentry *dentry){
    u8 name[MAX_FAT32_SHORT_FILE_NAME_LEN];
    u32 i;
    u32 j;
    u32 err;
    u32 found;
    u32 begin;
    u32 pagesize;
    u32 curPageNo;
    struct qstr                 qstr;
    struct qstr                 qstr2;
    struct inode                *dir;
    struct inode                *inode;
    struct condition            cond;
    struct vfs_page             *curPage;
    struct address_space        *mapping;
    struct fat_dir_entry        *fat_dir_entry;

    
    found = 0;
    inode = dentry->d_inode;
    dir = dentry->d_parent->d_inode;
    mapping = &(dir->i_data);
    pagesize = inode->i_blksize;

    for ( i = 0; i < dir->i_blocks; i++){
        curPageNo = mapping->a_op->bmap(dir, i);
        
        cond.cond1 = (void*)(&curPageNo);
        cond.cond2 = (void*)(dir);
        curPage = (struct vfs_page *)pcache->c_op->look_up(pcache, &cond);
        
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

        for ( begin = 0; begin < pagesize; begin += FAT32_DIR_ENTRY_LEN ){
            fat_dir_entry = (struct fat_dir_entry *)(curPage->p_data + begin);

            if (fat_dir_entry->attr == 0x08 || fat_dir_entry->attr == 0x0F)
                continue;
            
            if (fat_dir_entry->name[0] == 0xE5)
                continue;

            if (fat_dir_entry->name[0] == '\0')
                break;
            
            kernel_memset( name, 0, MAX_FAT32_SHORT_FILE_NAME_LEN * sizeof(u8) );
            for ( j = 0; j < MAX_FAT32_SHORT_FILE_NAME_LEN; j++ )
                name[j] = fat_dir_entry->name[j];
            qstr.name = name;
            qstr.len = MAX_FAT32_SHORT_FILE_NAME_LEN;

            fat32_convert_filename(&qstr2, &qstr, fat_dir_entry->lcase, FAT32_NAME_SPECIFIC_TO_NORMAL);

            if ( generic_compare_filename( &qstr2, &(dentry->d_name) ) == 0 ){
                fat_dir_entry->name[0] = 0xE5;
                found = 1;

                fat32_writepage(curPage);
                break;                          
            }
        }
        if (found)
            break;                              
    }

    if (!found){
        return -ENOENT;
    }
        

    return 0;
}

u32 fat32_write_inode(struct inode * inode, struct dentry * parent){
    u8 name[MAX_FAT32_SHORT_FILE_NAME_LEN];
    u32 i;
    u32 j;
    u32 err;
    u32 found;
    u32 begin;
    u32 curPageNo;
    u32 pagesize;
    struct qstr                             qstr;
    struct qstr                             qstr2;
    struct inode                            * dir;
    struct dentry                           * dentry;
    struct vfs_page                         * curPage;
    struct condition                        cond;
    struct address_space                    * mapping;
    struct fat_dir_entry                    * fat_dir_entry;

    found = 0;
    dir         = parent->d_inode;
    mapping     = &(dir->i_data);
    pagesize    = dir->i_blksize;
    
    dentry      = container_of(inode->i_dentry.next, struct dentry, d_alias);

    for ( i = 0; i < dir->i_blocks; i++){
        curPageNo = mapping->a_op->bmap(dir, i);
        cond.cond1 = (void*)(&curPageNo);
        cond.cond2 = (void*)(dir);
        curPage = (struct vfs_page *)pcache->c_op->look_up(pcache, &cond);

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
                return -ENOENT;
            }

            curPage->p_state = P_CLEAR;
            pcache->c_op->add(pcache, (void*)curPage);
            list_add(&(curPage->p_list), &(mapping->a_cache));
        }
        
        for ( begin = 0; begin < pagesize; begin += FAT32_DIR_ENTRY_LEN ){
            fat_dir_entry = (struct fat_dir_entry *)(curPage->p_data + begin);

            if (fat_dir_entry->attr == 0x08 || fat_dir_entry->attr == 0x0F)
                continue;
            
            if (fat_dir_entry->name[0] == 0xE5)
                continue;

            if (fat_dir_entry->name[0] == '\0')
                break;
            
            kernel_memset( name, 0, MAX_FAT32_SHORT_FILE_NAME_LEN * sizeof(u8) );
            for ( j = 0; j < MAX_FAT32_SHORT_FILE_NAME_LEN; j++ )
                name[j] = fat_dir_entry->name[j];

            qstr.name = name;
            qstr.len = MAX_FAT32_SHORT_FILE_NAME_LEN;

            fat32_convert_filename(&qstr2, &qstr, fat_dir_entry->lcase, FAT32_NAME_SPECIFIC_TO_NORMAL);

            if ( generic_compare_filename( &qstr2, &(dentry->d_name) ) == 0 ){
                fat_dir_entry->size         = inode->i_size;
                found = 1;
                break;                          
            }
        }
        if (found)
            break;                              
    }

    if (!found)
        return -ENOENT;

    err = mapping->a_op->writepage(curPage);
    if(err)
        return err;
    
    return 0;
}

struct dentry* fat32_inode_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd){  
    u8 name[MAX_FAT32_SHORT_FILE_NAME_LEN];
    u16 low, high;
    u32 addr;
    u32 i;
    u32 j;
    u32 k;
    u32 err;
    u32 found;
    u32 begin;
    u32 curPageNo;
    u32 pagesize;
    struct qstr                             qstr;
    struct qstr                             qstr2;
    struct vfs_page                         *curPage;
    struct condition                        cond;
    struct inode                            *new_inode;
    struct address_space                    *mapping;
    struct fat_dir_entry                    *fat_dir_entry;

    found = 0;
    new_inode = 0;
    mapping = &(dir->i_data);
    pagesize = dir->i_blksize;

    for ( i = 0; i < dir->i_blocks; i++){
        curPageNo = mapping->a_op->bmap(dir, i);
        cond.cond1 = (void*)(&curPageNo);
        cond.cond2 = (void*)(dir);
        curPage = (struct vfs_page *)pcache->c_op->look_up(pcache, &cond);
        
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
        

        for ( begin = 0; begin < pagesize; begin += FAT32_DIR_ENTRY_LEN ){
            
            fat_dir_entry = (struct fat_dir_entry *)(curPage->p_data + begin);

            if (fat_dir_entry->attr == 0x08 || fat_dir_entry->attr == 0x0F)
                continue;
            
            if (fat_dir_entry->name[0] == 0xE5)
                continue;

            if (fat_dir_entry->name[0] == '\0')
                break;
            
            kernel_memset( name, 0, MAX_FAT32_SHORT_FILE_NAME_LEN * sizeof(u8) );
            for ( j = 0; j < MAX_FAT32_SHORT_FILE_NAME_LEN; j++ )
                name[j] = fat_dir_entry->name[j];
            qstr.name = name;
            qstr.len = MAX_FAT32_SHORT_FILE_NAME_LEN;

            fat32_convert_filename(&qstr2, &qstr, fat_dir_entry->lcase, FAT32_NAME_SPECIFIC_TO_NORMAL);

            if ( generic_compare_filename( &qstr2, &(dentry->d_name) ) == 0 ){
                low     = fat_dir_entry->startlo;
                high    = fat_dir_entry->starthi;
                addr    = (high << 16) + low;

                new_inode = (struct inode*) kmalloc ( sizeof(struct inode) );
                new_inode->i_count          = 1;
                new_inode->i_ino            = addr;           
                new_inode->i_blkbits        = dir->i_blkbits;
                new_inode->i_blksize        = dir->i_blksize;
                new_inode->i_sb             = dir->i_sb;
                new_inode->i_size           = fat_dir_entry->size;
                new_inode->i_blocks         = 0;
                new_inode->i_fop            = &(fat32_file_operations);
                INIT_LIST_HEAD(&(new_inode->i_dentry));

                if ( fat_dir_entry->attr & ATTR_DIRECTORY )   
                    new_inode->i_op         = &(fat32_inode_operations[0]);
                else
                    new_inode->i_op         = &(fat32_inode_operations[1]);

                new_inode->i_data.a_host        = new_inode;
                new_inode->i_data.a_pagesize    = new_inode->i_blksize;
                new_inode->i_data.a_op          = &(fat32_address_space_operations);
                INIT_LIST_HEAD(&(new_inode->i_data.a_cache));

                
                while ( 0x0FFFFFFF != addr ){
                    new_inode->i_blocks ++;
                    addr = read_fat(new_inode, addr);         
                }

                new_inode->i_data.a_page        = (u32*) kmalloc (new_inode->i_blocks * sizeof(u32) );
                kernel_memset(new_inode->i_data.a_page, 0, new_inode->i_blocks);

                addr = new_inode->i_ino;
                for( k = 0; k < new_inode->i_blocks; k++){
                    new_inode->i_data.a_page[k] = addr;
                    addr = (new_inode, addr);
                }
                
                found = 1;
                break;                          
            }
        }
        if (found)
            break;                              
    }

    if (!found)
        return 0;

    dentry->d_inode = new_inode;
    dentry->d_op = &fat32_dentry_operations;
    list_add(&dentry->d_alias, &new_inode->i_dentry);  
    
    return dentry;
}


u32 fat32_create(struct inode *dir, struct dentry *dentry, u32 mode, struct nameidata *nd)
{
    return 0;
}

u32 fat32_readdir(struct file * file, struct getdent * getdent){
    u8 name[MAX_FAT32_SHORT_FILE_NAME_LEN];
    u32 i;
    u32 j;
    u32 err;
    u32 addr;
    u32 low;
    u32 high;
    u32 begin;
    u32 pagesize;
    u32 curPageNo;
    struct inode                    *dir;
    struct qstr                     qstr;
    struct qstr                     qstr2;
    struct condition                cond;
    struct fat_dir_entry            *fat_dir_entry;
    struct vfs_page                 *curPage;
    struct address_space            *mapping;

    dir = file->f_dentry->d_inode;
    mapping = &(dir->i_data);
    pagesize = dir->i_blksize;

    getdent->count = 0;
    getdent->dirent = (struct dirent *) kmalloc ( sizeof(struct dirent) * (dir->i_blocks * pagesize / FAT32_DIR_ENTRY_LEN));
    if (getdent->dirent == 0)
        return -ENOMEM;

    for ( i = 0; i < dir->i_blocks; i++){
        curPageNo = mapping->a_op->bmap(dir, i);
        if (curPageNo == 0)
            return -ENOENT;

        cond.cond1 = (void*)(&curPageNo);
        cond.cond2 = (void*)(dir);
        curPage = (struct vfs_page *)pcache->c_op->look_up(pcache, &cond);
        
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
      
        for ( begin = 0; begin < pagesize; begin += FAT32_DIR_ENTRY_LEN ){
            fat_dir_entry = (struct fat_dir_entry *)(curPage->p_data + begin);

            if (fat_dir_entry->attr == 0x08 || fat_dir_entry->attr == 0x0F)
                continue;
            
            if (fat_dir_entry->name[0] == 0xE5)
                continue;

            if (fat_dir_entry->name[0] == '\0')
                break;
            
            kernel_memset( name, 0, MAX_FAT32_SHORT_FILE_NAME_LEN * sizeof(u8) );
            for ( j = 0; j < MAX_FAT32_SHORT_FILE_NAME_LEN; j++ )
                name[j] = fat_dir_entry->name[j];
            
            qstr.name = name;
            qstr.len = MAX_FAT32_SHORT_FILE_NAME_LEN;

            fat32_convert_filename(&qstr2, &qstr, fat_dir_entry->lcase, FAT32_NAME_SPECIFIC_TO_NORMAL);

            low     = fat_dir_entry->startlo;
            high    = fat_dir_entry->starthi;
            addr    = (high << 16) + low;

            getdent->dirent[getdent->count].ino         = addr;
            getdent->dirent[getdent->count].name        = qstr2.name;  

            if ( fat_dir_entry->attr & ATTR_DIRECTORY )
                getdent->dirent[getdent->count].type    = FT_DIR;
            else
                getdent->dirent[getdent->count].type    = FT_REG_FILE;

            getdent->count += 1;
        }   
    }       

    return 0;
}

void fat32_convert_filename(struct qstr* dest, const struct qstr* src, u8 mode, u32 direction){
    u8* name;
    int i;
    u32 j;
    u32 dot;
    int end;
    u32 null;
    int dot_pos;

    dest->name = 0;
    dest->len = 0;

    if ( direction == FAT32_NAME_NORMAL_TO_SPECIFIC ){
        name = (u8 *) kmalloc ( MAX_FAT32_SHORT_FILE_NAME_LEN * sizeof(u8) );

        dot = 0;
        dot_pos = INF;
        for ( i = 0; i < src->len; i++ )
            if ( src->name[i] == '.' ){
                dot = 1;
                break;
            }
                
        if (dot)
            dot_pos = i;

        if ( dot_pos > MAX_FAT32_SHORT_FILE_NAME_BASE_LEN )
            end = MAX_FAT32_SHORT_FILE_NAME_BASE_LEN - 1;
        else
            end = dot_pos - 1;

        for ( i = 0; i < MAX_FAT32_SHORT_FILE_NAME_BASE_LEN; i++ ){
            if ( i > end )
                name[i] = '\0';
            else {
                if ( src->name[i] <= 'z' && src->name[i] >= 'a' )
                    name[i] = src->name[i] - 'a' + 'A';
                else
                    name[i] = src->name[i];
            }
        }

        for ( i = MAX_FAT32_SHORT_FILE_NAME_BASE_LEN, j = dot_pos + 1; i < MAX_FAT32_SHORT_FILE_NAME_LEN; i++, j++ )
        {
            if ( j >= src->len )
                name[i] == '\0';
            else{
                if ( src->name[j] <= 'z' && src->name[j] >= 'a' )
                    name[i] = src->name[j] - 'a' + 'A';
                else
                    name[i] = src->name[j];
            }
        }
        
        dest->name = name;
        dest->len = MAX_FAT32_SHORT_FILE_NAME_LEN;
    }

    else if ( direction == FAT32_NAME_SPECIFIC_TO_NORMAL ) {
        null = 0;
        dot_pos = MAX_FAT32_SHORT_FILE_NAME_LEN;
        for ( i = MAX_FAT32_SHORT_FILE_NAME_LEN - 1; i  ; i-- ){
            if ( src->name[i] == 0x20 ) {
                dot_pos = i;
                null ++;
            }

        }

        dest->len = MAX_FAT32_SHORT_FILE_NAME_LEN - null;
        name = (u8 *) kmalloc ( (dest->len + 2) * sizeof(u8) );     
        
        if ( dot_pos > MAX_FAT32_SHORT_FILE_NAME_BASE_LEN )
            dot_pos = MAX_FAT32_SHORT_FILE_NAME_BASE_LEN;
        
        for ( i = 0; i < dot_pos; i++ ) {
            if (src->name[i] <= 'z' && src->name[i] >= 'a' && (mode == 0x10 || mode == 0x00) )
                name[i] = src->name[i] - 'a' + 'A';
            else if (src->name[i] <= 'Z' && src->name[i] >= 'A' && (mode == 0x18 || mode == 0x08) )
                name[i] = src->name[i] - 'A' + 'a';
            else
                name[i] = src->name[i];
        }
        
        i = dot_pos;
        j = MAX_FAT32_SHORT_FILE_NAME_BASE_LEN;
        if (src->name[j] != 0x20){
            name[i] = '.';
            for ( i = dot_pos + 1; j < MAX_FAT32_SHORT_FILE_NAME_LEN && src->name[j] != 0x20; i++, j++ ){
                if (src->name[j] <= 'z' && src->name[j] >= 'a' && (mode == 0x08 || mode == 0x00) )
                    name[i] = src->name[j] - 'a' + 'A';
                else if (src->name[j] <= 'Z' && src->name[j] >= 'A' && (mode == 0x18 || mode == 0x10))
                    name[i] = src->name[j] - 'A' + 'a';
                else
                    name[i] = src->name[j];
            }
            dest->len += 1;
        }
        
        name[i] = '\0';
        dest->name = name;
    }
    else
        return;
}

u32 fat32_readpage(struct vfs_page *page){
    u32 err;
    u32 data_base;
    u32 abs_sect_addr;
    struct inode *inode;

    inode = page->p_mapping->a_host;
    data_base = ((struct fat32_basic_information *)(inode->i_sb->s_fs_info))->fa_FAT->data_sec;
    abs_sect_addr = data_base + (page->p_location - 2) * (inode->i_blksize >> SECTOR_SHIFT);

    page->p_data = ( u8* ) kmalloc ( sizeof(u8) * inode->i_blksize );
    if (page->p_data == 0)
        return -ENOMEM;

    err = read_block(page->p_data, abs_sect_addr, inode->i_blksize >> SECTOR_SHIFT);
    if (err)
        return -EIO;
    
    return 0;
}

u32 fat32_writepage(struct vfs_page *page){
    u32 err;
    u32 data_base;
    u32 abs_sect_addr;
    struct inode *inode;


    inode = page->p_mapping->a_host;


    data_base = ((struct fat32_basic_information *)(inode->i_sb->s_fs_info))->fa_FAT->data_sec;
    abs_sect_addr = data_base + (page->p_location - 2) * (inode->i_blksize >> SECTOR_SHIFT);

    err = write_block(page->p_data, abs_sect_addr, inode->i_blksize >> SECTOR_SHIFT);

    if (err)
        return -EIO;
    
    return 0;
}

u32 fat32_bmap(struct inode* inode, u32 pageNo){
    return inode->i_data.a_page[pageNo];
}

u32 read_fat(struct inode* inode, u32 index) {
    u8 buffer[SECTOR_SIZE];
    u32 shift;
    u32 base_sect;
    u32 dest_sect;
    u32 dest_index;
    struct fat32_basic_information * FAT32_BI;

    FAT32_BI = (struct fat32_basic_information *)(inode->i_sb->s_fs_info);
    base_sect = FAT32_BI->fa_FAT->base;

    shift = SECTOR_SHIFT - FAT32_FAT_ENTRY_LEN_SHIFT;
    dest_sect = base_sect + ( index >> shift );
    dest_index = index & (( 1 << shift ) - 1 );

    read_block(buffer, dest_sect, 1);
    return get_u32(buffer + (dest_index << FAT32_FAT_ENTRY_LEN_SHIFT));
}
