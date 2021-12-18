#ifndef util_h
#define util_h

#include "types.h"

int get_block(int dev, int blk, char *buf);
int put_block(int dev, int blk, char *buf);

void tokenize(char *pathname);
void split_path(char *pathname, char *dname, char *bname);
MINODE *mialloc();
int midalloc(MINODE *mip);

MINODE *iget(int dev, int ino);
void iput(MINODE *mip);
int search(MINODE *mip, char *name);
int getino(char *pathnames);
int findmyname(MINODE *parent_minode, u32 my_ino, char *my_name);
int findino(MINODE *mip, u32 *myino);
void enter_name(MINODE *pip, int ino, char *name);

#endif