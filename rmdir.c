#include "commands.h"

extern int dev;
extern SUPER *sp;
extern GD *gp;
extern int nblocks, ninodes, bmap, imap, iblk;

int isEmpty(MINODE *mip) {
  INODE *ip = &mip->INODE;

  if (ip->i_links_count > 2) return 0;

  char buf[BLKSIZE], temp[256];
  char *cp;
  DIR *dp;

  for (int i = 0; i < 12; i++) {
    if (ip->i_block[i] == 0) break;

    get_block(dev, ip->i_block[i], buf);
    dp = (DIR *)buf;
    cp = buf;

    while (cp < buf + BLKSIZE) {
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;

      if (strcmp(temp, ".") && strcmp(temp, "..") && strcmp(temp, "")) { // if it's not ".." or "." or "", return false
        return 0;
      }
      cp += dp->rec_len;
      dp = (DIR *)cp;
    }
  }
  return 1;
}

int clr_bit(char *buf, int bit) {
  buf[bit / 8] &= ~(1 << (bit % 8));
}

int incFreeInodes(int dev) {
  // inc free inodes count in SUPER and GD
  char buf[BLKSIZE];
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count++;
  put_block(dev, 1, buf);
  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count++;
  put_block(dev, 2, buf);
}

// allocate an inode number from inode_bitmap
void idalloc(int dev, int ino) {
  int i;
  char buf[BLKSIZE];

  if (ino > ninodes) {
    printf("Error: inumber %d out of range\n", ino);
    return;
  }

  get_block(dev, imap, buf); // get inode bitmap block into buf[]
  clr_bit(buf, ino - 1);     // clear bit ino-1 to 0
  put_block(dev, imap, buf); // write buf back
  incFreeInodes(dev);
}

void bdalloc(int dev, int blk) {
  int i;
  char buf[BLKSIZE];

  // read inode_bitmap block
  get_block(dev, bmap, buf);
  clr_bit(buf, blk - 1);
  put_block(dev, bmap, buf);
}

int rm_child(MINODE *pip, char *name) {
  char buf[BLKSIZE], temp[256];
  INODE *ip = &pip->INODE;
  DIR *dp;
  char *cp;
  char *cp2;

  // Search mip's direct blocks only
  for (int i = 0; i < 12; i++) {
    if (ip->i_block[i] == 0) return 0; // if the data block is empty, the rest of the data blocks will be empty

    get_block(pip->dev, ip->i_block[i], buf);
    dp = (DIR *)buf;
    cp = buf;

    while (cp < buf + BLKSIZE) {
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;

      if (strcmp(temp, name) == 0) {
        if (dp->rec_len == BLKSIZE) { // first and only entry in data block
          bdalloc(pip->dev, ip->i_block[i]);
          ip->i_size -= BLKSIZE;

          // For this case, where it is first and only entry in the data block,
          // we end up deleting the entire block at i_block[i]
          // so we need to fill in the gap if it there are blocks that come after it.

          // We don't have to check this for the other two cases, where the entry is
          // at the end or middle because there are still some entries left in the block

          for (; i < 11; i++) {
            if (ip->i_block[i + 1] == 0) break;

            get_block(pip->dev, ip->i_block[i + 1], buf);
            put_block(pip->dev, ip->i_block[i], buf);
          }
          ip->i_block[i] = 0;
        } else if (cp + dp->rec_len >= buf + BLKSIZE) { // last entry in block
          int rec_len = dp->rec_len;

          dp = (DIR *)cp2; // previous cp
          dp->rec_len += rec_len;
          put_block(pip->dev, ip->i_block[i], buf);
        } else {
          // need to move all the other entries to the left by 1
          cp2 = cp;
          DIR *dp2 = (DIR *)cp2;

          while (cp2 + dp2->rec_len < buf + BLKSIZE) {
            cp2 += dp2->rec_len;
            dp2 = (DIR *)cp2;
          }

          dp2->rec_len += dp->rec_len;
          memmove(cp, cp + dp->rec_len, (buf + BLKSIZE) - (cp + dp->rec_len));
          put_block(pip->dev, ip->i_block[i], buf);
        }
        return 1;
      }
      cp2 = cp;

      cp += dp->rec_len;
      dp = (DIR *)cp;
    }
  }
  return 0;
}

void my_rmdir(char *pathname) {
  if (pathname[0] == 0) return;

  char dname[128];
  char bname[128];
  split_path(pathname, dname, bname);

  int pino = getino(dname);
  if (!pino) {
    printf("Error: Failed to find an ino for directory %s\n", dname);
    return;
  }

  MINODE *pip = iget(dev, pino);

  if (!pip) {
    printf("Error: Couldn't load inode %d\n\n", pino);
    return;
  }

  if (!S_ISDIR(pip->INODE.i_mode)) {
    printf("Error: %s is not a directory\n\n", dname);
    iput(pip);
    return;
  }

  int ino = search(pip, bname);
  MINODE *mip = iget(dev, ino);

  if (mip->refCount > 2) {
    printf("Error: Cannot rmdir %s. Directory still in use by other users\n\n", pathname);
    iput(mip);
    return;
  }

  // Check if it's empty
  if (!isEmpty(mip)) {
    printf("Error: Cannot rmdir %s. Directory is not empty\n\n", pathname);
    iput(mip);
    return;
  }

  // deallocate its data blocks and inode
  for (int i = 0; i < 12; i++) {
    if (mip->INODE.i_block[i] == 0) break;
    bdalloc(mip->dev, mip->INODE.i_block[i]);
  }
  idalloc(mip->dev, mip->ino);
  iput(mip);

  rm_child(pip, bname);
  pip->INODE.i_links_count -= 1;
  pip->dirty = 1;
  pip->INODE.i_atime = time(0L);
  pip->INODE.i_mtime = time(0L);
  iput(pip);

  printf("Removed directory %s\n\n", pathname);
}