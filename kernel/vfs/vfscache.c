#include <zjunix/vfs/vfscache.h>

#include <zjunix/log.h>
#include <zjunix/slab.h>
#include <zjunix/utils.h>
#include <driver/vga.h>

struct cache * dcache;
struct cache * pcache;
struct cache * icache;

struct cache_operations dentry_cache_operations = {
    .look_up    = dcache_look_up,
    .add        = dcache_add,
    .is_full    = cache_is_full,
};

struct cache_operations inode_cache_operations = {
    .is_full    = cache_is_full,
};

struct cache_operations page_cache_operations = {
    .look_up    = pcache_look_up,
    .add        = pcache_add,
    .is_full    = cache_is_full,
    .write_back = pcache_write_back,
};

u32 init_cache(){
    u32 err;

    err = -ENOMEM;
    dcache = (struct cache*) kmalloc ( sizeof(struct cache) );
    if (dcache == 0)
        goto init_cache_err;

    cache_init(dcache, DCACHE_CAPACITY, DCACHE_HASHTABLE_SIZE);
    dcache->c_op = &dentry_cache_operations;

    icache = (struct cache*) kmalloc ( sizeof(struct cache) );
    if (icache == 0)
        goto init_cache_err;

    cache_init(icache, ICACHE_CAPACITY, ICACHE_HASHTABLE_SIZE);
    icache->c_op = &inode_cache_operations;

    pcache = (struct cache*) kmalloc ( sizeof(struct cache) );
    if (pcache == 0)
        goto init_cache_err;

    cache_init(pcache, PCACHE_CAPACITY, PCACHE_HASHTABLE_SIZE);
    pcache->c_op = &page_cache_operations;

init_cache_ok:
    return 0;

init_cache_err:
    if (icache) {
        kfree(icache->c_hashtable);
        kfree(icache);
    }
    if (dcache) {
        kfree(dcache->c_hashtable);
        kfree(dcache);
    }
    return err;
}

void cache_init(struct cache* this, u32 capacity, u32 tablesize){
    u32 i;

    this->c_size = 0;
    this->c_capacity = capacity;
    this->c_tablesize = tablesize;
    INIT_LIST_HEAD(&(this->c_LRU));
    this->c_hashtable = (struct list_head*) kmalloc ( tablesize * sizeof(struct list_head) );
    for ( i = 0; i < tablesize; i++ )
        INIT_LIST_HEAD(this->c_hashtable + i);
    this->c_op = 0;
}

u32 cache_is_full(struct cache* this){
    return this->c_size == this->c_capacity;
}

void* dcache_look_up(struct cache *this, struct condition *cond) {
    u32 found;
    u32 hash;
    struct qstr         *name;
    struct qstr         *qstr;
    struct dentry       *parent;
    struct dentry       *tested;
    struct list_head    *current;
    struct list_head    *start;
    parent  = (struct dentry*) (cond->cond1);
    name    = (struct qstr*) (cond->cond2);
   
    hash = __stringHash(name, this->c_tablesize);
    current = &(this->c_hashtable[hash]);
    start = current;

    found = 0;
    while ( current->next != start ){
        current = current->next;
        tested = container_of(current, struct dentry, d_hash);
        qstr = &(tested->d_name);
        if ( !parent->d_op->compare(qstr, name) && tested->d_parent == parent ){
            found = 1;
            break;
        }
    }

    if (found) {
        list_del(&(tested->d_hash));
        list_add(&(tested->d_hash), &(this->c_hashtable[hash]));
        list_del(&(tested->d_LRU));
        list_add(&(tested->d_LRU), &(this->c_LRU));
        return (void*)tested;
    }
    else{
        return 0;
    }
}

void dcache_add(struct cache *this, void *object){
    u32 hash;
    struct dentry* addend;

    addend = (struct dentry *) object;
    hash = __stringHash(&addend->d_name, this->c_tablesize);

    if (cache_is_full(this))
        dcache_put_LRU(this);

    list_add(&(addend->d_hash), &(this->c_hashtable[hash]));

    list_add(&(addend->d_LRU), &(this->c_LRU));

    this->c_size += 1;
}

void dcache_put_LRU(struct cache *this){
    struct list_head        *put;
    struct list_head        *start;
    struct dentry           *least_ref;
    struct dentry           *put_dentry;

    start = &(this->c_LRU);
    put_dentry = container_of(put, struct dentry, d_LRU);

    put = start->prev;
    while ( put != start && put_dentry->d_count > 0 ){
        put = put->prev;
        put_dentry = container_of(put, struct dentry, d_LRU);
    }

    if (put == start){
        least_ref = 0;
        put = start->prev;
        while ( put != start ){
            put_dentry = container_of(put, struct dentry, d_LRU);
            if (put_dentry->d_pinned & D_PINNED)
                continue;
            
            if (least_ref == 0)
                least_ref = put_dentry;
            else if(least_ref->d_count > put_dentry->d_count)
                least_ref = put_dentry;

            put = put->prev;
        }
        put_dentry = least_ref;
    }

    list_del(&(put_dentry->d_LRU));
    list_del(&(put_dentry->d_hash));
    list_del(&(put_dentry->d_child));
    list_del(&(put_dentry->d_alias));
    list_del(&(put_dentry->d_subdirs));
    this->c_size -= 1;

    release_dentry(put_dentry);
}

void* icache_look_up(struct cache *this, struct condition *cond) {
    // u32 found = 0;
    // u32 ino;
    // u32 
    // u32 hash = __intHash(cond->cond_number, this->c_tablesize);
    
    // struct inode        *tested;
    // struct list_head    *current;
    // struct list_head    *start;
    // current = &(this->c_hashtable[hash]);
    // start = current;

    // while ( current->next != start ){
    //     current = current->next;
    //     tested = container_of(current, struct dentry, i_hash);
    //     if ( tested->i_ino == cond->cond_number
    //          && tested->i_sb->s_type == cond->cond_other ){   // inode number and file system type are enough for identifying
    //         found = 1;
    //         break;
    //     }
    // }
    // if(found)
    //     return (void*)tested;
    // else
    //     return 0;
    return 0;
}

void icache_add(struct cache *this, void *object){
    // u32 hash;
    // struct inode* addend;

    // addend = (struct inode*) object;
    // hash = __intHash(addend->i_ino, this->c_tablesize);

    // if (cache_is_full(this))                                    // if the cache is full
    //     icache_put_LRU(this);                                   // need to replace an inode

    // list_add(&(addend->i_hash), &(this->c_hashtable[hash]));    // insert into the hash table
    
    // list_add(&(addend->i_LRU), &(this->c_LRU));                 // also insert into the LRU list
    // this->c_size += 1;
}

void icache_put_LRU(struct cache *this){
    // struct list_head* put;
    // struct inode* put_inode;

    // put = this->i_LRU->prev;  
    // put_inode = container_of(put, struct inode, i_LRU);

    // if(put_inode->i_state & I_DIRTY){
    //     if(put_inode->i_state & I_DIRTY_DATASYNC)
    //         pcache_write((void*)(put_inode->i_mapping));     // write back the file data
    //     put_inode->i_sb->write_inode(put_inode);            // write back the inode
    // }

    // list_del(put);                              // remove from LRU list
    // list_del(&(put_inode->i_hash));             // remove from hash table
    // this->c_size -= 1;

    // kfree(put_inode);                           // remove from the memory physically
}

void* pcache_look_up(struct cache *this, struct condition *cond) {
    u32 found = 0;
    u32 hash;
    u32 pageNum;
    struct inode        *inode;
    struct vfs_page     *tested;
    struct list_head    *current;
    struct list_head    *start;
    
    pageNum = *((u32*)(cond->cond1));
    inode = (struct inode *)(cond->cond2);

    hash = __intHash(pageNum, this->c_tablesize);
    current = &(this->c_hashtable[hash]);
    start = current;

    while ( current->next != start ){
        current = current->next;
        tested = container_of(current, struct vfs_page, p_hash);

        if ( tested->p_location == pageNum && tested->p_mapping->a_host == inode ){
            found = 1;
            break;
        }
    }
    

    if (found) {
        list_del(&(tested->p_hash));
        list_add(&(tested->p_hash), start);
        list_del(&(tested->p_LRU));
        list_add(&(tested->p_LRU), &(this->c_LRU));
        return (void*)tested;
    }
    else
        return 0;
}

void pcache_add(struct cache *this, void *object){
    u32 hash;
    struct vfs_page *addend;

    addend = (struct vfs_page *) object;
    hash = __intHash(addend->p_location, this->c_tablesize);

    if (cache_is_full(this)) 
        pcache_put_LRU(this);

    list_add(&(addend->p_hash), &(this->c_hashtable[hash]));

    list_add(&(addend->p_LRU), &(this->c_LRU));

    this->c_size += 1;
}

void pcache_put_LRU(struct cache *this){
    struct list_head    *put;
    struct vfs_page     *put_page;

    put = this->c_LRU.prev;
    put_page = container_of(put, struct vfs_page, p_LRU);

    if(put_page->p_state & P_DIRTY)
        this->c_op->write_back((void *)put_page);

    list_del(&(put_page->p_LRU));
    list_del(&(put_page->p_hash));
    list_del(&(put_page->p_list));
    this->c_size -= 1;

    release_page(put_page);
}

void pcache_write_back(void *object){
    struct vfs_page *current;
    current = (struct vfs_page *) object;
    current->p_mapping->a_op->writepage(current);
}

void dget(struct dentry *dentry){
    dentry->d_count += 1;
}

void dput(struct dentry *dentry){
    dentry->d_count -= 1;
}

void release_dentry(struct dentry *dentry){
    kfree(dentry);
}

void release_inode(struct inode *inode){

    kfree(inode);
}

void release_page(struct vfs_page* page){
    kfree(page->p_data);
    kfree(page);
}

u32 __intHash(u32 key, u32 size){
    u32 mask = size - 1;                      
    return key & mask;
}

u32 __stringHash(struct qstr * qstr, u32 size){
    u32 i = 0;
    u32 value = 0;
    for (i = 0; i < qstr->len; i++)
        value = value * 31 + (u32)(qstr->name[i]);           

    u32 mask = size - 1;                   
    return value & mask;
}
