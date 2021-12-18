#include "commands.h"

extern int fd, dev;

// Change filename’s permission bits to octal value
void my_chmod(int oct, char *filename) {
  int ino = getino(filename);
  if (!ino) {
    printf("Error: Failed to find an ino for file %s\n", filename);
    return;
  }

  MINODE *mip = iget(dev, ino);
  if (mip) {
    mip->INODE.i_mode |= oct;
    mip->dirty = 1;
  }
  iput(mip);
}

// change file’s access time to current time
void my_utime(char *filename) {
  int ino = getino(filename);
  if (!ino) {
    printf("Failed to find an ino for file %s\n", filename);
    return;
  }

  MINODE *mip = iget(dev, ino);
  if (mip) {
    mip->INODE.i_atime = time(0L);
    mip->dirty = 1;
  }
  iput(mip);
}

/*
void stat(char *filename) {
  struct stat myst;
  int ino = getino(filename);
  MINODE *mip = iget(dev, ino);

  // copy mip->INODE fields to myst fields;
  INODE *ip = &mip->INODE;
  myst.st_dev = dev;
  memcpy(myst.st_ino, ino);
  myst.st_mode = ip->i_mode;
  myst.st_nlink = 

  myst.st_uid = ip->i_uid;
  myst.st_gid = ip->i_gid;

  myst.st_rdev =
  myst.st_size = ip->i_size;
  myst.st_blksize = 
  myst.st_blocks = ip->i_blocks;

  myst.st_atime = 
  myst.st_mtime =
  myst.st_ctime = 
  iput(mip);
}
*/