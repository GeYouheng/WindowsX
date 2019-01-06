#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>

#include <zjunix/utils.h>
#include <zjunix/slab.h>
#include <driver/vga.h>
#include <zjunix/log.h>

extern struct cache     *dcache;
extern struct dentry    *root_dentry;
extern struct dentry    *pwd_dentry;
extern struct vfsmount  *root_mnt;
extern struct vfsmount  *pwd_mnt;

struct file* vfs_open(const u8 *filename, u32 flags, u32 mode){
    u32 namei_flags;
    u32 err;
    struct file         *f;
    struct nameidata    nd;

    namei_flags = flags;                                 
    if ( (namei_flags + 1) & O_ACCMODE )
        namei_flags ++;
    
    err = open_namei(filename, namei_flags, mode, &nd);

    if (!err)
    {
    	return dentry_open(nd.dentry, nd.mnt, flags);
	} 

    return ERR_PTR(err);
}

u32 open_namei(const u8 *pathname, u32 flag, u32 mode, struct nameidata *nd){
    u32 err;
    u32 acc_mode;
    struct path     path;
    struct dentry   *dir;
    struct dentry   *dentry;

    acc_mode = ACC_MODE(flag);
    if (flag & O_APPEND)                
        acc_mode |= MAY_APPEND;
    
    nd->intent.open.flags = flag;       // TOKNOW
    nd->intent.open.create_mode = mode;

    if (!(flag & O_CREAT)) {
        err = path_lookup(pathname, LOOKUP_FOLLOW, nd);
        if (err)
            return err;
        goto ok;
    }   
    
    if ( err = path_lookup(pathname, LOOKUP_PARENT|LOOKUP_CREATE, nd) )
        return err;
    

	err = -EISDIR;
	if (nd->last_type != LAST_NORM || nd->last.name[nd->last.len])
		goto exit;

	dir = nd->dentry;
	nd->flags &= ~LOOKUP_PARENT;
	dentry = __lookup_hash(&nd->last, nd->dentry, nd);

do_last:
	err = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto exit;

	if (!dentry->d_inode) {
        err = dir->d_inode->i_op->create(dir->d_inode, dentry, mode, nd);
        dput(nd->dentry);
		nd->dentry = dentry;
		if (err)
			goto exit;

		acc_mode = 0;
		goto ok;
	}

	err = -ENOENT;
	if (!dentry->d_inode)
		goto exit_dput;

	dput(nd->dentry);
	nd->dentry = dentry;
ok:
	return 0;
exit_dput:
    dput(dentry);
exit:
	dput(nd->dentry);
	return err;

}

struct dentry * __lookup_hash(struct qstr *name, struct dentry *base, struct nameidata *nd) {
    u32 err;
	struct dentry   *dentry;
	struct inode    *inode;

    inode = base->d_inode;
    struct condition cond;
    cond.cond1 = (void*) nd->dentry;
    cond.cond2 = (void*) name;
    dentry = (struct dentry*) dcache->c_op->look_up(dcache, &cond);

	if (!dentry) {
        struct dentry *new = d_alloc(base, name);
		dentry = ERR_PTR(-ENOMEM);
		if (!new)
            return dentry;
        
        
		dentry = inode->i_op->lookup(inode, new, nd);
		if (!dentry)
			dentry = new;   
		else
			dput(new);
	}

	return dentry;
}

u32 path_lookup(const u8 * name, u32 flags, struct nameidata *nd) {
    nd->last_type = LAST_ROOT;
    nd->flags = flags;

    
    if ( *name == '/' ) {
        dget(root_dentry);
        nd->mnt     = root_mnt;
        nd->dentry  = root_dentry;
    }
    
    else {
        dget(pwd_dentry);
        nd->mnt     = pwd_mnt;
        nd->dentry  = pwd_dentry;
    }
    
    return link_path_walk(name, nd);
}

u32 link_path_walk(const u8 *name, struct nameidata *nd) {
    u32 err;
    struct path     next;
    struct inode    *inode;
    u32 lookup_flags = nd->flags;

    while (*name=='/' || *name == ' ' )  
        name++;
    if (!(*name))
        goto return_reval;

    inode = nd->dentry->d_inode;

    for( ; ; ){
        u8 c;
        struct qstr this;
        
        this.name = name;
        do {  
            name++;      
            c = *(const u8 *)name;  
        } while (c && (c != '/'));  
        this.len = name - (const u8*) this.name;

        if (!c)  
            goto last_component;
        
        while (*++name == '/');

        if (!*name)
            goto last_with_slashes;

        if (this.name[0] == '.') switch (this.len) {  
            default:
                break;

            case 2:
                if (this.name[1] != '.')  
                    break;  
                follow_dotdot(&nd->mnt, &nd->dentry);  
                inode = nd->dentry->d_inode;

            case 1:  
                continue;
        }

        nd->flags |= LOOKUP_CONTINUE;               
        err = do_lookup(nd, &this, &next);         
        if (err)                                  
            break;

        follow_mount(&next.mnt, &next.dentry);

        err = -ENOENT;
        inode = next.dentry->d_inode;

		if (!inode){
            goto out_dput;
        }
			
        err = -ENOTDIR; 
		if (!inode->i_op)
            goto out_dput;
            
        dput(nd->dentry);
        nd->mnt = next.mnt;
        nd->dentry = next.dentry;

        err = -ENOTDIR;
		if (!inode->i_op->lookup)
            break;
            
        continue;

last_with_slashes:
        lookup_flags |= LOOKUP_FOLLOW | LOOKUP_DIRECTORY;

last_component:
        nd->flags &= ~LOOKUP_CONTINUE;             

		if (lookup_flags & LOOKUP_PARENT)         
            goto lookup_parent;

		if (this.name[0] == '.') switch (this.len) {
			default:
                break;
                
			case 2:	
				if (this.name[1] != '.')
					break;
				follow_dotdot(&nd->mnt, &nd->dentry);
				inode = nd->dentry->d_inode;
            
			case 1:
				goto return_reval;
        }
            
		err = do_lookup(nd, &this, &next);
		if (err)
            break;

        follow_mount(&next.mnt, &next.dentry);

        dput(next.dentry);
        nd->mnt = next.mnt;
		nd->dentry = next.dentry;

        inode = nd->dentry->d_inode;

		err = -ENOENT;
		if (!inode){
            break;
        }
        
		if (lookup_flags & LOOKUP_DIRECTORY) {
			err = -ENOTDIR; 
			if (!inode->i_op || !inode->i_op->lookup)
				break;
        }
            
        //无错误返回
        goto return_base;
        
lookup_parent:
        nd->last = this;
        
		nd->last_type = LAST_NORM;
		if (this.name[0] != '.')
            goto return_base;
        
		if (this.len == 1)
			nd->last_type = LAST_DOT;
		else if (this.len == 2 && this.name[1] == '.')
			nd->last_type = LAST_DOTDOT;
		else
            goto return_base;
        
return_reval:
	
return_base:
		return 0;
out_dput:
        dput(next.dentry);
		break;
    }
    dput(nd->dentry);
return_err:
    return err;
}

inline void follow_dotdot(struct vfsmount **mnt, struct dentry **dentry){

    while(1) {
		struct vfsmount *parent;
		struct dentry *old = *dentry;

		if (*dentry == root_dentry && *mnt == root_mnt )
			break;
        
		if (*dentry != (*mnt)->mnt_root) {
            *dentry = (*dentry)->d_parent;
            dget(*dentry);
            dput(old);
			break;
		}
        
        parent = (*mnt)->mnt_parent;
		if (parent == *mnt)
			break;

        *dentry = (*mnt)->mnt_mountpoint;
        dget(*dentry);
        dput(old);
        *mnt = parent;
    }
}

u32 do_lookup(struct nameidata *nd, struct qstr *name, struct path *path){
    
    struct vfsmount *mnt = nd->mnt;
    struct condition cond;
    cond.cond1 = (void*) nd->dentry;
    cond.cond2 = (void*) name;
    struct dentry* dentry = (struct dentry*) dcache->c_op->look_up(dcache, &cond); 
    
    if (!dentry)
        goto need_lookup;  

done:  
    path->mnt = mnt;
    path->dentry = dentry;
    dget(dentry);
    return 0;  
  
need_lookup:
    dentry = real_lookup(nd->dentry, name, nd);
    if (IS_ERR(dentry))
        goto fail;
    goto done;
  
fail:
    return PTR_ERR(dentry);
}

struct dentry * real_lookup(struct dentry *parent, struct qstr *name, struct nameidata *nd){
    struct dentry   *result;
    struct inode    *dir = parent->d_inode;

    struct dentry *dentry = d_alloc(parent, name);
    result = ERR_PTR(-ENOMEM);

	if (dentry) {
        result = parent->d_inode->i_op->lookup(dir, dentry, nd);
        
		if (result)
			dput(dentry);
		else                    
			result = dentry;
    }
    
    return result;
}

struct file * dentry_open(struct dentry *dentry, struct vfsmount *mnt, u32 flags) {
	struct file *f;
	struct inode *inode;
	u32 error;

	error = -ENFILE;
	f = (struct file* ) kmalloc ( sizeof(struct file) );
    INIT_LIST_HEAD(&f->f_list);
	if (!f)
        goto cleanup_dentry;
    
    inode           = dentry->d_inode;

	f->f_flags      = flags;
	f->f_mode       = ((flags+1) & O_ACCMODE) | FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE ;
	f->f_mapping    = &(inode->i_data);
	f->f_dentry     = dentry;
	f->f_vfsmnt     = mnt;
	f->f_pos        = 0;
	f->f_op         = inode->i_fop;
	f->f_flags      &= ~(O_CREAT);

	return f;

cleanup_all:
	f->f_dentry     = 0;
    f->f_vfsmnt     = 0;

cleanup_file:
    list_del_init(&f->f_list);
    kfree(f);

cleanup_dentry:
    dput(dentry);
    return ERR_PTR(error);
}

u32 vfs_close(struct file *filp) {
	u32 err;
  
	err = filp->f_op->flush(filp);
    if (!err)
        kfree(filp);

	return err;
}

struct dentry * d_alloc(struct dentry *parent, const struct qstr *name){  
    u8* dname;
    u32 i;
    struct dentry* dentry;  
    
    dentry = (struct dentry *) kmalloc ( sizeof(struct dentry) );
    if (!dentry)  
        return 0;
    
    dname = (u8*) kmalloc ( (name->len + 1)* sizeof(u8*) );
    kernel_memset(dname, 0, (name->len + 1));
    for ( i = 0; i < name->len; i++ ){
        dname[i] = name->name[i];
    }
    dname[i] == '\0';


    dentry->d_name.name         = dname;
    dentry->d_name.len          = name->len;   
    dentry->d_count             = 1;
    dentry->d_inode             = 0;  
    dentry->d_parent            = parent;
    dentry->d_sb                = parent->d_sb;
    dentry->d_op                = 0;
    
    INIT_LIST_HEAD(&dentry->d_hash);  
    INIT_LIST_HEAD(&dentry->d_LRU);  
    INIT_LIST_HEAD(&dentry->d_subdirs);
    INIT_LIST_HEAD(&(root_dentry->d_alias));

    if (parent) {
        dentry->d_parent = parent;
        dget(parent);
        dentry->d_sb = parent->d_sb;
        list_add(&dentry->d_child, &parent->d_subdirs);
	} else {
		INIT_LIST_HEAD(&dentry->d_child);
	}

    dcache->c_op->add(dcache, (void*)dentry);
    return dentry;
}
