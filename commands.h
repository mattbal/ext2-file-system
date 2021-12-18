// One giant header file. Prevents us from having to make a header file
// for every single .c file, which would clutter the project

#ifndef commands_h
#define commands_h

#include "util.h"

// cd_ls_pwd
void cd(char *pathname);
void ls(char *pathname);
void pwd(MINODE *wd);
void rpwd(MINODE *wd);

// mkdir
void my_mkdir(char *pathname);
void kmkdir(MINODE *pmip, char *bname);
int ialloc(int dev);
int balloc(int dev);
void my_creat(char *pathname, int mode);
void kcreat(MINODE *pmip, char *bname, int mode);

// rmdir
void my_rmdir(char *pathname);
int rm_child(MINODE *pip, char *name);
void idalloc(int dev, int ino);
void bdalloc(int dev, int blk);

// link_unlink
void my_link(char *old_file, char *new_file);
void my_unlink(char *filename);

// symlink
void my_symlink(char *old_file, char *new_file);
int my_readlink(char *file, char *buffer);

// open_close_lseek
OFT *oftalloc();
void oftdalloc(OFT *o);
int my_open(char *filename, int flags);
int my_truncate(MINODE *mip);
int close_file(int fd);
int my_lseek(int fd, int position);

// read_cat
int my_read(int fd, char *buf, int nbytes);
int my_cat(char *filename);

// write_cp
int my_write(int fd, char buf[], int nbytes);
int my_cp(char *source, char *destination);

// mount_unmount
int my_mount(char *filesys, char *mount_point);
int my_unmount(char *filesys);

// misc
void my_chmod(int oct, char *filename);
void my_utime(char *filename);

#endif