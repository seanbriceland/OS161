/*
 * filetable.c
 * SPB & FAR
 * filetable helper functions
 *   1) filetable_init
 * 	2) add_filehandle
 */
#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <copyinout.h>
#include <uio.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <vnode.h>
#include <syscall.h>
#include <test.h>
#include <synch.h>
#include <filetable.h>
#include <openfile.h>



/*
 * filetable_init
 * initializes a filetable with stdin, stdout, stderr defaults at 0,1,2
 *
 * Returns 0 on success, error code on failure
 */
int filetable_init(){
	int err;
	char con1[5] = "con:";

	struct vnode *vn1, *vn2, *vn3;

	/////////////////////////////////////////////
	//stdin
	struct openfile *ofile1;
	ofile1 = kmalloc(sizeof(struct openfile));
	if (ofile1 == NULL)
		return ENOMEM;

	err = vfs_open(con1, O_RDONLY, 0664, &vn1);
	if(err)
		return err;

	err = openfile_init(vn1, O_RDONLY, ofile1);
	if(err)//check for err upon ofile init.
		return err;

	curthread->filetable[0] = ofile1;

	strcpy(con1, "con:"); //reinitilize con
	/////////////////////////////////////////////
	//stdout..
	err = vfs_open(con1, O_WRONLY, 0664, &vn2);//TODO vnode?
	if (err)
		return err;

	struct openfile *ofile2;
	ofile2 = kmalloc(sizeof(struct openfile));
	if (ofile2 == NULL)
		return ENOMEM;

	err = openfile_init(vn1, O_WRONLY, ofile2);
	if(err)//check for err upon ofile init.
		return err;

	curthread->filetable[1] = ofile2;

	strcpy(con1, "con:"); //reinitilize con
	/////////////////////////////////////////////
	//stderr...
	err = vfs_open(con1, O_WRONLY, 0664, &vn3);
	if (err)
		return err;
	
	struct openfile *ofile3;
	ofile3 = kmalloc(sizeof(struct openfile));
	if (ofile3 == NULL)
		return ENOMEM;

	err = openfile_init(vn1, O_WRONLY, ofile3);
	if(err)//check for err upon ofile init.
		return err;

	curthread->filetable[2] = ofile3;

	return 0; 
}

/**
 * add_filehandle
 * finds the next NULL entry in the filetable
 * sets the ofile to this entry
 * 
 * ON SUCCESS:
 * return the index where we stored the of_ptr.
 *
 * ON FAILURE:
 * set err
 */
int add_filehandle(struct openfile* ofile)
{
	for(int fd=3; fd<OPEN_MAX; fd++)
	{
		if(curthread->filetable[fd] == NULL)
		{
			curthread->filetable[fd] = ofile;
			return fd;
		}
	}
	return -1; // if this function reaches this line then the file table is full!
}
