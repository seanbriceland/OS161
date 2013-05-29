/*
 * openfile.c
 * SPB & FAR
 * Helper function for openfile struct
 *   1) openfile_init()
 * 
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <synch.h>
#include <openfile.h>

/**
 * Initializes a struct openfile.
 * Upon successfull vfsopen(), we need to initialize an openfile
 * 		to reference it in the FileTable.
 *
 * return ofile - type:struct
 */
int openfile_init(struct vnode* vn, int mode, struct openfile* ofile){
	ofile->vn_ptr = vn;
	ofile->mode = mode;

	ofile->refcount = 1;
	ofile->offset = 0;
	
	ofile->vnode_lock = lock_create("of_lock");
	if(ofile->vnode_lock == NULL)
		return ENOMEM;

	return 0;
}
