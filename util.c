#include "commands.h"

extern char gpath[128];
extern char *name[64];
extern int n;

extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, iblk;
extern char line[128], cmd[32], pathname[128];

extern MINODE minodes[NMINODE];
extern MINODE *root;
extern MOUNT mountTable[NMTABLE];
extern MOUNT *mp;
extern OFT oft[NOFT];
extern PROC procs[NPROC];
extern PROC *running;

int get_block(int dev, int blk, char *buf) {
  lseek(dev, (long)blk * BLKSIZE, 0); // move dev, the file descriptor, to the beginning of a block. Ex: 0 -> 1024, 1024 -> 2048, etc.
  read(dev, buf, BLKSIZE);            // read the block's data and put it into buf
}

int put_block(int dev, int blk, char *buf) {
  lseek(dev, (long)blk * BLKSIZE, 0);
  write(dev, buf, BLKSIZE);
}

void tokenize(char *pathname) {
  strcpy(gpath, pathname); // store tokens in the global variable gpath
  n = 0;

  char *s = strtok(gpath, "/");
  while (s) {
    name[n] = s;
    n++;
    s = strtok(0, "/");
  }
  name[n] = 0;
}

// break up a path into it's directory name and base name
void split_path(char *pathname, char *dname, char *bname) {
  int index = -1;
  int i = 0;

  // find last forward slash so we can separate directory name and base name
  while (pathname[i]) {
    if (pathname[i] == '/') {
      index = i;
    }
    i++;
  }

  if (index != -1) { // found slash
    if (index == 0) {
      strncpy(dname, pathname, index + 1);
      dname[index + 1] = 0; // add null character to end
    } else {
      strncpy(dname, pathname, index);
      dname[index] = 0; // add null character to end
    }
    strcpy(bname, pathname + index + 1);
  } else {
    dname[0] = 0;
    strcpy(bname, pathname);
  }
}

// allocate a FREE minode for use
MINODE *mialloc() {
  for (int i = 0; i < NMINODE; i++) {
    MINODE *mp = &minodes[i];
    if (mp->refCount == 0) {
      mp->refCount = 1;
      return mp;
    }
  }
  printf("FS panic: out of minodes\n");
  return 0;
}

// release a used minode
int midalloc(MINODE *mip) {
  mip->refCount = 0;
}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino) {
  MINODE *mip;
  int i;

  for (i = 0; i < NMINODE; i++) {
    mip = &minodes[i];
    if (mip->refCount && mip->dev == dev && mip->ino == ino) {
      mip->refCount++;
      //printf("found [%d %d] as minodes[%d] in core\n", dev, ino, i);
      return mip;
    }
  }

  for (i = 0; i < NMINODE; i++) {
    mip = &minodes[i];
    if (mip->refCount == 0) {
      //printf("allocating NEW minodes[%d] for [%d %d]\n", i, dev, ino);
      mip->refCount = 1;
      mip->dev = dev;
      mip->ino = ino;

      // get INODE of ino to buf
      int blk = (ino - 1) / 8 + iblk;
      int offset = (ino - 1) % 8;

      //printf("iget: ino=%d blk=%d offset=%d\n", ino, blk, offset);

      char buf[BLKSIZE];
      get_block(dev, blk, buf);
      INODE *ip = (INODE *)buf + offset;
      mip->INODE = *ip; // copy INODE to mp->INODE
      return mip;
    }
  }
  printf("PANIC: no more free minodes\n");
  return 0;
}

// release mip, a used minode.
void iput(MINODE *mip) {
  if (mip == 0) return;

  mip->refCount--;

  if (mip->refCount > 0) return;
  if (!mip->dirty) return;

  int block = (mip->ino - 1) / 8 + iblk;
  int offset = (mip->ino - 1) % 8;

  char buf[BLKSIZE];
  get_block(mip->dev, block, buf);
  INODE *ip = (INODE *)buf + offset; // point at inode
  *ip = mip->INODE;                  // copy inode to inode in block
  put_block(mip->dev, block, buf);   // write back to disk
  midalloc(mip);
}

// Search for an inode with the given name
int search(MINODE *mip, char *name) {
  char buf[BLKSIZE], temp[256];
  INODE *ip = &mip->INODE;
  DIR *dp;
  char *cp;

  // Search mip's direct blocks only
  for (int i = 0; i < 12; i++) {
    if (ip->i_block[i] == 0) return 0; // if the data block is empty, the rest of the data blocks will be empty

    get_block(mip->dev, ip->i_block[i], buf);
    dp = (DIR *)buf;
    cp = buf;

    // printf("  ino   rlen  nlen  name\n");
    while (cp < buf + BLKSIZE) {
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;
      // printf("%4d  %4d  %4d    %s\n", dp->inode, dp->rec_len, dp->name_len, dp->name);
      if (strcmp(temp, name) == 0) {
        // printf("found %s : ino = %d\n", temp, dp->inode);
        return dp->inode;
      }
      cp += dp->rec_len;
      dp = (DIR *)cp;
    }
  }
  return 0;
}

// returns the (dev, ino) of a pathname
int getino(char *pathname) {
  if (strcmp(pathname, "/") == 0) return 2;

  MINODE *mip;

  if (pathname[0] == '/') {
    mip = root;
  } else {
    mip = running->cwd;
  }

  mip->refCount++; // because we iput(mip) later
  tokenize(pathname);

  int ino = 0;

  for (int i = 0; i < n; i++) {
    // printf("===========================================\n");
    // printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);

    ino = search(mip, name[i]);

    if (ino == 0) {
      iput(mip);
      return 0;
    }
    iput(mip);
    mip = iget(dev, ino);
  }

  iput(mip);
  if (n == 0) return mip->ino;
  return ino;
}

// search parent's data block for myino; SAME as search() but by myino
int findmyname(MINODE *pip, u32 my_ino, char *my_name) {
  char buf[BLKSIZE], temp[256];
  INODE *ip = &pip->INODE;
  DIR *dp;
  char *cp;

  // printf("search for %d in MINODE = [%d, %d]\n", my_ino, parent_minode->dev, parent_minode->ino);
  for (int i = 0; i < 12; i++) {
    if (ip->i_block[i] == 0) return 0;

    get_block(pip->dev, ip->i_block[i], buf);
    dp = (DIR *)buf;
    cp = buf;
    // printf("  ino   rlen  nlen  name\n");

    while (cp < buf + BLKSIZE) {
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;

      // printf("%4d  %4d  %4d    %s\n", dp->inode, dp->rec_len, dp->name_len, dp->name);
      if (dp->inode == my_ino) {
        // printf("found %s : ino = %d\n", temp, dp->inode);
        strcpy(my_name, temp);
        return 1;
      }
      cp += dp->rec_len;
      dp = (DIR *)cp;
    }
  }
  return 0;
}

// myino = i# of . return i# of ..
int findino(MINODE *mip, u32 *myino) {
  // mip points at a DIR minode
  *myino = search(mip, ".");
  return search(mip, "..");
}

void enter_name(MINODE *pip, int ino, char *name) {
  char buf[BLKSIZE];
  INODE *ip = &pip->INODE;
  int needed_length = 4 * ((8 + strlen(name) + 3) / 4);
  int ideal_length = 0;
  int blk;
  char *cp;
  DIR *dp;

  for (int i = 0; i < 12; i++) {
    if (ip->i_block[i] == 0) { // no space in existing data blocks
      // Allocate a new data block; increment parent size by BLKSIZE;
      // Enter new entry as the first entry in the new data block with rec_len = BLKSIZE.
      blk = balloc(dev);
      ip->i_block[i] = blk;
      ip->i_size = BLKSIZE;
      pip->dirty = 1;

      get_block(dev, blk, buf);
      dp = (DIR *)buf;
      cp = buf;

      strcpy(dp->name, name);
      dp->inode = ino;
      dp->rec_len = BLKSIZE;
      dp->name_len = strlen(name);

      // write data block to disk
      put_block(dev, blk, buf);
      return;
    }

    get_block(pip->dev, ip->i_block[i], buf);
    dp = (DIR *)buf;
    cp = buf;
    ideal_length = 4 * ((8 + dp->name_len + 3) / 4);

    while (cp + dp->rec_len < buf + BLKSIZE) {
      cp += dp->rec_len;
      dp = (DIR *)cp;
      ideal_length = 4 * ((8 + dp->name_len + 3) / 4);
    }

    int remain = dp->rec_len - ideal_length;

    if (remain >= needed_length) {
      dp->rec_len = ideal_length;
      cp += dp->rec_len;
      dp = (DIR *)cp; // now points at new last entry

      strcpy(dp->name, name);
      dp->inode = ino;
      dp->rec_len = remain;
      dp->name_len = strlen(name);

      // write data block to disk
      put_block(dev, ip->i_block[i], buf);
      return;
    }
  }
}