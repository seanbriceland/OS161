/*
 * openfile.h
 * SPB & FAR
 * Header file for Openfile functions
 * Declaration of openfile stuct
 */

#ifndef OPENFILE_H_
#define OPENFILE_H_

////////////////////////
//OpenFile Struct
struct openfile{
  int refcount; // Reference count
	int mode; //flag for r, w, rw, etc..
	off_t offset; //unsigned int 64 bits
	struct lock* vnode_lock;
	struct vnode* vn_ptr;
};

int openfile_init(struct vnode* vn, int mode, struct openfile* ofile);

#endif /* OPENFILE_H_ */
