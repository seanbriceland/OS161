/*
 * filetable.h
 * SPB & FAR
 * Header file for filetable functions
 * struct filetable declaration
 */

#ifndef FILETABLE_H_
#define FILETABLE_H_

#include <limits.h>


///////////////////////////////////////
//Filetable Struct
struct filetable{
	struct openfile *ftarr[OPEN_MAX];
	int index;
};

int filetable_init(void);
int add_filehandle(struct openfile* ofile);
struct filetable filetable_create(void);
struct openfile* filetable_copy(void);

#endif /* FILETABLE_H_ */
