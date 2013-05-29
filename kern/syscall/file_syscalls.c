/*
 * file_syscalls.c
 * SPB & FAR
 * Implementation for all pertinent file syscalls
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
#include <kern/seek.h>
#include <stat.h>
#include "filetable.h"
#include "openfile.h"

/**
 * sys_open
 * RETURN int fd on success, -1 on failure
 */
int sys_open(const char *filepath, int flags, int *retv){
  int err=0;
	char kbuf[PATH_MAX];
	size_t got;

	err = copyinstr((userptr_t)filepath,kbuf,PATH_MAX,&got);  //Sanitize userptr_t
	if(err != 0)
		return err;

	struct vnode* vn;
	err = vfs_open(kbuf, flags, 0, &vn);
	if (err)
		return err;

	//After successful vfs_open...

	struct openfile *ofile;
	ofile = kmalloc(sizeof(struct openfile));
	if (ofile == NULL) {
		err = ENOMEM;
		goto fail;
	}
	*retv = add_filehandle(ofile);
	if(*retv == -1){
		err = EMFILE;
		goto fail1;
	}

	err = openfile_init(vn, flags, ofile);
	if (err)//check for err upon ofile init.
		goto fail2;
	
	return 0;
fail2:
	vfs_close(vn);
fail1:
	kfree(ofile);
fail:
	return err;
}

/**
 * sys_close
 * On success, close returns 0. On error an errNO is returned
 */
int sys_close(int fd, int* retv){
	struct lock *lkptr;
	int flag = 0;

	if(fd < 0 || fd >= OPEN_MAX){
		return EBADF;
	}

	if(curthread->filetable[fd]==NULL){
		*retv = -1;
		return EBADF;
	}

	struct openfile* ofile = curthread->filetable[fd];
	lock_acquire(ofile->vnode_lock);
	lkptr = ofile->vnode_lock;
	ofile->refcount--;

	if(ofile->refcount == 0){
		vfs_close(ofile->vn_ptr);
		kfree(ofile);
		flag = 1;
	}


	curthread->filetable[fd] = NULL;
	*retv = 0;
	lock_release(lkptr);
	if(flag ==1){
		lock_destroy(lkptr);
	}
	return 0;//success
}

/**
 * sys_read
 * ON SUCCESS - Returns number of bytes read.  Return of 0 signifies end of file.
 * ON FAILURE - Returns -1 and sets err to the appropriate error code.
 */
int sys_read(int fd, void *buf, size_t buflen, int *retv)
{
	int numbytes, err;
	struct iovec iov;
	struct uio userio;
	err = 0;

	if(fd < 0 || fd >= OPEN_MAX || curthread->filetable[fd]==NULL)
		return EBADF;

	struct openfile* ofile = curthread->filetable[fd];

	lock_acquire(ofile->vnode_lock);

	if(ofile->mode == O_WRONLY){  //fd does not exist, or file is not open for reading.
		lock_release(ofile->vnode_lock);
		return EBADF;
	}

	uio_uinit(&iov, &userio, (userptr_t)buf, buflen, ofile->offset, UIO_READ);  //Might need to user our uio_uinit() function here.
	err = VOP_READ(ofile->vn_ptr, &userio); //Does the actual reading
	if(err){
		lock_release(ofile->vnode_lock);
		return err;
	}

	numbytes = buflen - userio.uio_resid; //calculate number of bytes read.
	if(err){
		lock_release(ofile->vnode_lock);
		return err;
	} 
	ofile->offset = userio.uio_offset;
	lock_release(ofile->vnode_lock);
	*retv = numbytes;
	return 0; //SUCCESS

}

/**
 * sys_write
 * ON SUCCESS - Returns number of bytes written.  Return of 0 signifies nothing could be written, but no ERROR.
 * ON FAILURE - Returns -1 and sets err to the appropriate error code.
 */
int sys_write(int fd, const void *buf, size_t nbytes, int *retv)
{
	int numbytes, err;
	struct iovec iov;
	struct uio userio;
	//char* kbuf = (char*)kmalloc(nbytes);

	if(fd < 0 || fd >= OPEN_MAX || curthread->filetable[fd]==NULL)
		return EBADF;

	struct openfile* ofile = curthread->filetable[fd];//gets ofile
	lock_acquire(ofile->vnode_lock);
	if(ofile->mode == O_RDONLY){  //fd does not exist, or file is not open for reading.
		lock_release(ofile->vnode_lock);
		return EBADF;
	}

	uio_uinit(&iov, &userio, (userptr_t)buf, nbytes, ofile->offset, UIO_WRITE);  //TODO Might need to user our uio_uinit() function here.

	err = VOP_WRITE(ofile->vn_ptr, &userio); //Does the actual reading
	if(err){
		lock_release(ofile->vnode_lock);
		return err;
	}
	numbytes = nbytes - userio.uio_resid;  //calculate bytes read
	ofile->offset = userio.uio_offset;
	lock_release(ofile->vnode_lock);
	*retv = numbytes;
	return 0; //SUCCESS!
}
/*
 * sys_lseek
 * lseek alters the current seek position of the file handle filehandle,
 * seeking to a new position based on pos and whence. If whence is
 * SEEK_SET, the new position is pos.
 * SEEK_CUR, the new position is the current position plus pos.
 * SEEK_END, the new position is the position of end-of-file plus pos.
 * anything else, lseek fails.
 * NOTE:: that pos is a signed quantity.
 * It is not meaningful to seek on certain objects (such as the console device). All seeks on these objects fail.
 * Seek positions less than zero are invalid. Seek positions beyond EOF are legal.
 * NOTE:: that each distinct open of a file should have an independent seek pointer.
 */
int sys_lseek(int fd, off_t pos, int whence, off_t* lsret)
{
	struct openfile *ofile;
	struct stat fstat;
	int err = 0;
	off_t temp = 0;

	if(fd < 0 || fd >= OPEN_MAX || curthread->filetable[fd]==NULL) //checks to make sure fd is a valid file handle.
		return EBADF;

	ofile = curthread->filetable[fd];  //gets ofile
	lock_acquire(ofile->vnode_lock);   //gets lock

	switch(whence){
		case SEEK_SET://SEEK_SET, the new position is pos.
			err = VOP_TRYSEEK(ofile->vn_ptr, pos);
			if(err){
				lock_release(ofile->vnode_lock);
				return err;
			}
			ofile->offset = pos;
		break;

		case SEEK_CUR: //SEEK_CUR, the new position is the current position plus pos.
			temp = ofile->offset + pos;
			err = VOP_TRYSEEK(ofile->vn_ptr,temp);
			if(err){
				lock_release(ofile->vnode_lock);
				return err;
			}
			ofile->offset = temp;
		break;

		case SEEK_END: //SEEK_END, the new position is the position of end-of-file plus pos. 
			err = VOP_STAT(curthread->filetable[fd]->vn_ptr, &fstat);
			if(err){
				lock_release(ofile->vnode_lock);
				return err;
			}
			temp = fstat.st_size + pos;
			err = VOP_TRYSEEK(ofile->vn_ptr,temp);
			if(err){
				lock_release(ofile->vnode_lock);
				return err;
			}
			ofile->offset = temp;
		break;
		///If case is not recognized.  This should fall through....I think?
		default:
			lock_release(ofile->vnode_lock);
			return EINVAL;
	}
	//SUCCESS
	*lsret = ofile->offset;
	lock_release(ofile->vnode_lock);
	return 0;
}

/**
 * sys_dup2
 * ON SUCCESS sets reval = newfd and returns 0 to error
 * ON ERROR -1 is returned, and errno is set according to the error encountered.
 */
int sys_dup2(int oldfd, int newfd, int* retv)
{
	int err = 0;

	//Both filehandles must be non-negative, less than OPEN_MAX and oldfd must be a valid file handle.
	if(oldfd < 0 || oldfd >= OPEN_MAX || newfd < 0 || newfd >= OPEN_MAX || curthread->filetable[oldfd]==NULL)
		return EBADF;

	if(oldfd == newfd){ //No need to process a dup of the same fd.. Return 0 to act as if it worked :)
		*retv = newfd;
		return 0;
	}

	//If newfd names an open file, that file is closed.
	if(curthread->filetable[newfd] != NULL){
		err = sys_close(newfd, retv);
		if(err)//TODO SHOULD NEVER be true  SINCE ONLY CHECK IS FOR NULL at [newfd]
			return err;
	}

	//clones the file handle oldfd onto the file handle newfd
	struct openfile* dup_ofile = curthread->filetable[oldfd];
	lock_acquire(dup_ofile->vnode_lock);
	curthread->filetable[newfd] = dup_ofile;
	curthread->filetable[newfd]->refcount++; // adding a ref to ofile.
	*retv = newfd;
	lock_release(dup_ofile->vnode_lock);
	return 0;
}

/**
 * ON SUCCESS: chdir returns 0.
 * ON ERROR: -1 is returned, and errno is set according to the error encountered.
 */
int sys_chdir(const char *pathname)
{
	int err=0;
	char clean_path[PATH_MAX];
	size_t got;

	if(pathname == NULL)
		return EFAULT;

	err = copyinstr((userptr_t)pathname,clean_path,PATH_MAX,&got);  //Sanitize pathname
	if(err)
		return err;

	//The current directory of the current process is set to the directory named by pathname.
	err = vfs_chdir(clean_path);
	if(err)
		return err;

	return 0;
}

/**
 * ON SUCCESS: __getcwd returns the length of the data returned
 * ON ERROR: -1 is returned, and errno is set according to the error encountered.
 */
int sys___getcwd(char *buf, size_t buflen, int *retv)
{
	int  err = 0;
	struct iovec iov;
	struct uio userio;
	off_t offt;//TODO - How do we correctly pass in an offset if we don't need to use one?
	uio_uinit(&iov, &userio, (userptr_t)buf, buflen, offt, UIO_READ);
	err = vfs_getcwd(&userio);//returns our errors for us.
	if(err)
		return err;

	*retv = buflen - userio.uio_resid;//TODO return the length of data returned.
	return 0;
}
