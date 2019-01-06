#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>

#include <zjunix/slab.h>
#include <zjunix/utils.h>
#include <driver/vga.h>
#include <zjunix/log.h>

extern struct cache * pcache;

u32 vfs_read(struct file *file, char *buf, u32 count, u32 *pos) {
	u32 ret;

	if (!(file->f_mode & FMODE_READ))
		return -EBADF;
	if (!file->f_op || (!file->f_op->read))
        return -EINVAL;
        			
	ret = file->f_op->read(file, buf, count, pos);

	return ret;
}

u32 vfs_write(struct file *file, char *buf, u32 count, u32 *pos) {
	u32 ret;

	if (!(file->f_mode & FMODE_WRITE))
		return -EBADF;
	if (!file->f_op || (!file->f_op->write))
        return -EINVAL;
   	
	ret = file->f_op->write(file, buf, count, pos);	
    return ret;
}

u32 generic_file_read(struct file *file, u8 *buf, u32 count, u32 *ppos){

    u32 pos;
    u32 start;
    u32 blksize;
    u32 pageNo;
    u32 startPageNo;
    u32 endPageNo;
    u32 cur;
    u32 startPageCur;
    u32 endPageCur;
    u32 readCount;
    u32 r_page;
    struct condition    cond;
    struct inode        *inode;
    struct vfs_page     *curPage;
    struct address_space *mapping;
   
    inode = file->f_dentry->d_inode;
    mapping = &(inode->i_data);

    pos = *ppos;
    blksize = inode->i_blksize;
    startPageNo = pos / blksize;
    startPageCur = pos % blksize;
    if (pos + count < inode->i_size){
        endPageNo = ( pos + count ) / blksize;
        endPageCur = ( pos + count ) % blksize;
    }
    else{
        endPageNo = inode->i_size / blksize;
        endPageCur = inode->i_size % blksize;
    }

    cur = 0;
    for ( pageNo = startPageNo; pageNo <= endPageNo; pageNo ++){
        r_page = mapping->a_op->bmap(inode, pageNo);           

        cond.cond1 = (void*)(&r_page);
        cond.cond2 = (void*)(file->f_dentry->d_inode);
        curPage = (struct vfs_page *) pcache->c_op->look_up(pcache, &cond);

        if ( curPage == 0 ){
            curPage = (struct vfs_page *) kmalloc ( sizeof(struct vfs_page) );
            if (!curPage)
                goto out;

            curPage->p_state = P_CLEAR;
            curPage->p_location = r_page;
            curPage->p_mapping = mapping;
            INIT_LIST_HEAD(&(curPage->p_hash));
            INIT_LIST_HEAD(&(curPage->p_LRU));
            INIT_LIST_HEAD(&(curPage->p_list));

            if ( mapping->a_op->readpage(curPage) ){
                release_page(curPage);
                goto out;
            }

            pcache->c_op->add(pcache, (void*)curPage);
            list_add(&(curPage->p_list), &(mapping->a_cache));
        }

        if ( pageNo == startPageNo ){
            if ( startPageNo == endPageNo ){
                readCount = endPageCur - startPageCur;
                kernel_memcpy(buf, curPage->p_data + startPageCur, readCount);
            }
            else {
                readCount = blksize - startPageCur;
                kernel_memcpy(buf, curPage->p_data + startPageCur, readCount);
            }
        }
        else if ( pageNo == endPageNo ){
            readCount = endPageCur;
            kernel_memcpy(buf + cur, curPage->p_data, readCount);
        }
        else {
            readCount = blksize;
            kernel_memcpy(buf + cur, curPage->p_data, readCount);
        }

        cur += readCount;
        *ppos += readCount;
    }

out:
    file->f_pos = *ppos;
    return cur;
}

u32 generic_file_write(struct file *file, u8 *buf, u32 count, u32 *ppos){
    u32 pos;
    u32 start;
    u32 blksize;
    u32 pageNo;
    u32 startPageNo;
    u32 endPageNo;
    u32 cur;
    u32 startPageCur;
    u32 endPageCur;
    u32 writeCount;
    u32 r_page;
    struct condition    cond;
    struct dentry       *parent;
    struct inode        *inode;
    struct vfs_page     *curPage;
    struct address_space *mapping;
   
    inode = file->f_dentry->d_inode;
    mapping = &(inode->i_data);

    pos = *ppos;
    blksize = inode->i_blksize;
    startPageNo = pos / blksize;
    startPageCur = pos % blksize;
 
    endPageNo = ( pos + count ) / blksize;
    endPageCur = ( pos + count ) % blksize;

    
    cur = 0;
    for ( pageNo = startPageNo; pageNo <= endPageNo; pageNo ++){
        r_page = mapping->a_op->bmap(inode, pageNo);               

        cond.cond1 = (void*)(&r_page);
        cond.cond2 = (void*)(file->f_dentry->d_inode);
        curPage = (struct vfs_page *) pcache->c_op->look_up(pcache, &cond);

        if ( curPage == 0 ){
            curPage = (struct vfs_page *) kmalloc ( sizeof(struct vfs_page) );
            if (!curPage)
                goto out;

            curPage->p_state = P_CLEAR;
            curPage->p_location = r_page;
            curPage->p_mapping = mapping;
            INIT_LIST_HEAD(&(curPage->p_hash));
            INIT_LIST_HEAD(&(curPage->p_LRU));
            INIT_LIST_HEAD(&(curPage->p_list));

            if ( mapping->a_op->readpage(curPage) ){
                release_page(curPage);
                goto out;
            }

            pcache->c_op->add(pcache, (void*)curPage);
            list_add(&(curPage->p_list), &(mapping->a_cache));
        }

        if ( pageNo == startPageNo ){
            if ( startPageNo == endPageNo ){
                writeCount = endPageCur - startPageCur;
                kernel_memcpy(curPage->p_data + startPageCur, buf + cur, writeCount);
            }
            else {
                writeCount = blksize - startPageCur;
                kernel_memcpy(curPage->p_data + startPageCur, buf + cur, writeCount);
            }
        }
        else if ( pageNo == endPageNo ){
            writeCount = endPageCur;
            kernel_memcpy(curPage->p_data, buf + cur, writeCount);
        }
        else {
            writeCount = blksize;
            kernel_memcpy(curPage->p_data, buf + cur, writeCount);
        }

        mapping->a_op->writepage(curPage);
        
        cur += writeCount;
        *ppos += writeCount;

    }

    if (pos + count > inode->i_size) {
        inode->i_size = pos + count;
        parent = file->f_dentry->d_parent;
        inode->i_sb->s_op->write_inode(inode, parent);
    }

out:
    file->f_pos = *ppos;
    return cur;
}

u32 generic_file_flush(struct file * file){

    struct vfs_page *page;
    struct inode *inode;
    struct list_head *a, *begin;
    struct address_space *mapping;
        
    inode = file->f_dentry->d_inode;
    mapping = &(inode->i_data);
    begin = &(mapping->a_cache);
    a = begin->next;

    while ( a != begin ){
        page = container_of(a, struct vfs_page, p_list);
        if ( page->p_state & P_DIRTY ){
            mapping->a_op->writepage(page);
        }
        a = a->next;
    }
        
    return 0;
}
