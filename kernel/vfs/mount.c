#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>

#include <zjunix/log.h>
#include <zjunix/slab.h>
#include <driver/vga.h>
#include <zjunix/utils.h>

extern struct dentry            * root_dentry;
extern struct vfsmount          * root_mnt;

u32 mount_ext2(){
    u32 err;
    struct      qstr            qstr;
    struct      dentry          * dentry;
    struct      vfsmount        * mnt;
    struct      list_head       * a;
    struct      list_head       * begin;

    a = &(root_mnt->mnt_hash);
    begin = a;
    a = a->next;
    while ( a != begin ){
        mnt = container_of(a, struct vfsmount, mnt_hash);
        if ( kernel_strcmp(mnt->mnt_sb->s_type->name ,"ext2") == 0 )
            break;
    }
        
    if ( a == begin ) 
        return -ENOENT;
    
    qstr.name = "ext2";
    qstr.len = 4;
    

    dentry = d_alloc(root_dentry, &qstr);
    if (dentry == 0)
        return -ENOENT;

    dentry->d_mounted = 1;
    dentry->d_inode = mnt->mnt_root->d_inode;
    mnt->mnt_mountpoint = dentry;
    mnt->mnt_parent = root_mnt;
    
    return 0;
}

u32 follow_mount(struct vfsmount **mnt, struct dentry **dentry){
    u32 res = 0;

    while ((*dentry)->d_mounted) {
		struct vfsmount *mounted = lookup_mnt(*mnt, *dentry);
		if (!mounted)
			break;

        *mnt = mounted;
        dput(*dentry);
        *dentry = mounted->mnt_root;
        dget(*dentry);
		res = 1;
    }
    
    return res;
}

struct vfsmount * lookup_mnt(struct vfsmount *mnt, struct dentry *dentry) {
	struct list_head *head = &(mnt->mnt_hash);
	struct list_head *tmp = head;
	struct vfsmount *p, *found = 0;

	for (;;) {
		tmp = tmp->next;
		p = 0;
		if (tmp == head)
			break;
		p = list_entry(tmp, struct vfsmount, mnt_hash);
		if (p->mnt_parent == mnt && p->mnt_mountpoint == dentry) {
			found = p;
			break;
		}
	}
	
	return found;
}
