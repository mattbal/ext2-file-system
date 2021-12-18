#include "commands.h"

extern int dev;

void my_link(char *old_file, char *new_file) {
  // verify old file exits and is not a DIR
  int oldIno = getino(old_file);
  if (!oldIno) {
    printf("Error: Failed to find an ino for file %s\n\n", old_file);
    return;
  }

  MINODE *oldMip = iget(dev, oldIno);
  if (!oldMip) {
    printf("Error: Couldn't load inode %d\n\n", oldIno);
    return;
  }

  if (!S_ISDIR(oldMip->INODE.i_mode)) {
    int newIno = getino(new_file);

    if (newIno == 0) { // file does not exist
      char parent[128];
      char child[128];
      split_path(new_file, parent, child);

      int parentIno = getino(parent);
      MINODE *parentMip = iget(dev, parentIno);

      enter_name(parentMip, oldIno, child); // create entry in parent DIR with same inode # as old_file
      oldMip->INODE.i_links_count++;
      oldMip->dirty = 1;
      iput(oldMip);
      iput(parentMip);
      printf("Link created\n\n");
      return;
    }
  }
  iput(oldMip); // need to decrease refCount that was increased by iget
}

void my_unlink(char *filename) {
  int ino = getino(filename);
  if (!ino) {
    printf("Error: Failed to find an ino for file %s\n\n", filename);
    return;
  }

  MINODE *mip = iget(dev, ino);
  if (!mip) {
    printf("Error: Couldn't load inode %d\n\n", ino);
    return;
  }

  if (!S_ISDIR(mip->INODE.i_mode)) {

    char parent[128];
    char child[128];
    split_path(filename, parent, child);

    int parentIno = getino(parent);
    MINODE *parentMip = iget(dev, parentIno);

    rm_child(parentMip, child);
    parentMip->dirty = 1;

    iput(parentMip);
    mip->INODE.i_links_count--; // decrease the link since one of file's deleted

    if (mip->INODE.i_links_count > 0) {
      mip->dirty = 1;
    } else {
      // deallocate all data blocks inode
      bdalloc(dev, mip->ino);
      // deallocate INODE
      idalloc(dev, mip->ino);
    }
  }
  iput(mip);
}