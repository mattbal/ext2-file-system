#include "commands.h"

extern int fd, dev;

void my_symlink(char *old_file, char *new_file) {
  // verify old file exits and new file does not exist yet
  int oldIno = getino(old_file);
  if (!oldIno) {
    printf("Error: Failed to find an ino for file %s\n", old_file);
    return;
  }

  // create link type file
  // my_creat's code has been copy and pasted here
  // because we need to do too many custom modifications

  if (new_file[0] == 0) return;

  char dname[128];
  char bname[128];
  split_path(new_file, dname, bname);

  printf("dirname = %s\nbasename = %s\n", dname, bname);
  printf("Checking if parent directory %s exists ...\n", dname);

  int ino = getino(dname);
  if (!ino) {
    printf("Error: Failed to find an ino for directory %s\n", dname);
    return;
  }

  MINODE *pip = iget(dev, ino);
  if (!pip) {
    printf("Error: Couldn't load inode %d\n\n", ino);
    return;
  }

  if (!S_ISDIR(pip->INODE.i_mode)) {
    printf("Error: %s is not a directory\n", dname);
    iput(pip);
    return;
  }

  printf("Checking if %s already exists...\n", bname);
  if (search(pip, bname)) {
    printf("Error: Failed to create file at %s - %s already exists.\n", dname, bname);
    iput(pip);
    return;
  }

  kcreat(pip, bname, 0120644);
  pip->INODE.i_links_count = 1;
  pip->dirty = 1;
  iput(pip);

  // need to increase refCount manually because we're about to modify pip again
  // and write pip's blocks to the disk w/ iput on line 84
  // Normally we wouldn't have to do this, iget increases refCount, but to be more efficient
  // we're not going to call iget again just to increase refCount
  pip->refCount++;

  // update file that has been made
  int newIno = getino(new_file);
  if (!newIno) {
    printf("Error: Failed to find an ino for file %s\n", new_file);
    iput(pip);
    return;
  }

  MINODE *mip = iget(dev, newIno);

  if (mip) {
    char dname[128];
    char bname[128];
    split_path(old_file, dname, bname);

    INODE *ip = &mip->INODE;
    ip->i_size = strlen(bname); // assume old name is <= 60 chars
    memcpy(ip->i_block, bname, 60);
    mip->dirty = 1;
    iput(mip);

    pip->dirty = 1;
    printf("Created a symbolic link\n\n");
  }
  iput(pip);
}

int my_readlink(char *file, char *buffer) {
  int ino = getino(file);
  if (!ino) {
    printf("Error: Failed to find an ino for file %s\n", file);
    return 0;
  }

  MINODE *mip = iget(dev, ino);
  if (!mip) {
    printf("Error: Couldn't load inode %d\n\n", ino);
    return 0;
  }

  if (S_ISLNK(mip->INODE.i_mode)) {
    strcpy(buffer, (char *)mip->INODE.i_block);
    iput(mip);
    return strlen(buffer);
  }

  iput(mip);
  return 0;
}
