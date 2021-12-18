#include "commands.h"

extern char gpath[128];
extern char *name[64];
extern int dev;
extern PROC *running;
extern SUPER *sp;
extern GD *gp;
extern MOUNT mountTable[NMTABLE];
extern MOUNT *mp;
extern int nblocks, ninodes, bmap, imap, iblk;

void my_mkdir(char *pathname) {
  if (pathname[0] == 0) return;

  char dname[128];
  char bname[128];
  split_path(pathname, dname, bname);

  printf("dirname = %s\nbasename = %s\n", dname, bname);
  printf("Checking if parent directory %s exists ...\n", dname);

  int ino = getino(dname);
  if (!ino) {
    printf("Error: Failed to find an ino for directory %s\n", dname);
    return;
  }

  MINODE *mip = iget(dev, ino);
  if (!mip) {
    printf("Error: Couldn't load inode %d\n\n", ino);
    return;
  }

  if (!S_ISDIR(mip->INODE.i_mode)) {
    printf("Error: %s is not a directory\n", dname);
    iput(mip);
    return;
  }

  printf("Checking if %s already exists...\n", bname);
  if (search(mip, bname)) {
    printf("Error: Failed to create directory at %s - %s already exists.\n", dname, bname);
    iput(mip);
    return;
  }

  kmkdir(mip, bname);
  mip->INODE.i_links_count++; // .. is a link to this parent minode
  mip->dirty = 1;
  iput(mip);
  printf("Created a new directory\n\n");
}

int tst_bit(char *buf, int bit) {
  return buf[bit / 8] & (1 << (bit % 8));
}

int set_bit(char *buf, int bit) {
  buf[bit / 8] |= (1 << (bit % 8));
}

int decFreeInodes(int dev) {
  // dec free inodes count in SUPER and GD
  char buf[BLKSIZE];
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count--;
  put_block(dev, 1, buf);
  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count--;
  put_block(dev, 2, buf);
}

// allocate an inode number from inode_bitmap
int ialloc(int dev) {
  char buf[BLKSIZE];

  // read inode_bitmap block
  get_block(dev, imap, buf);

  for (int i = 0; i < ninodes; i++) {
    if (tst_bit(buf, i) == 0) {
      set_bit(buf, i);
      put_block(dev, imap, buf);
      decFreeInodes(dev);
      printf("allocated ino = %d\n", i + 1); // bits count from 0; ino from 1
      return i + 1;
    }
  }
  return 0;
}

int balloc(int dev) {
  char buf[BLKSIZE];

  // read inode_bitmap block
  get_block(dev, bmap, buf);

  for (int i = 0; i < nblocks; i++) {
    if (tst_bit(buf, i) == 0) {
      set_bit(buf, i);
      put_block(dev, bmap, buf);
      return i + 1;
    }
  }
  return 0;
}

void kmkdir(MINODE *pmip, char *bname) {
  // create INODE
  int ino = ialloc(dev);
  int blk = balloc(dev);

  MINODE *mip = iget(dev, ino);
  INODE *ip = &mip->INODE;
  ip->i_mode = 040755;
  ip->i_block[0] = blk;
  for (int i = 1; i < 14; i++) {
    ip->i_block[i] = 0;
  }
  ip->i_links_count = 2;
  ip->i_uid = running->uid;
  ip->i_gid = running->gid;
  ip->i_size = BLKSIZE;
  ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
  ip->i_blocks = 2;
  mip->dirty = 1; // Marked as dirty - node has been changed
  iput(mip);

  // create data block with "." and ".."
  char buf[BLKSIZE];
  DIR *dp = (DIR *)buf;
  char *cp = buf;
  strcpy(dp->name, ".");
  dp->inode = ino;
  dp->rec_len = 12;
  dp->name_len = 1;

  // Move to next block and write ".."
  cp += dp->rec_len;
  dp = (DIR *)cp;

  strcpy(dp->name, "..");
  dp->inode = pmip->ino;
  dp->rec_len = BLKSIZE - 12; // make second block take up entire block length
  dp->name_len = 2;

  put_block(dev, blk, buf);
  enter_name(pmip, ino, bname);
}

void my_creat(char *pathname, int mode) {
  if (pathname[0] == 0) return;

  char dname[128];
  char bname[128];
  split_path(pathname, dname, bname);

  printf("dirname = %s\nbasename = %s\n", dname, bname);
  printf("Checking for directory %s ...\n", dname);

  int ino = getino(dname);
  if (!ino) {
    printf("Error: Failed to find an ino for directory %s\n", dname);
    return;
  }

  MINODE *mip = iget(dev, ino);

  if (!mip) {
    printf("Error: Couldn't load inode %d\n\n", ino);
    return;
  }

  if (!S_ISDIR(mip->INODE.i_mode)) {
    printf("Error: %s is not a directory\n", dname);
    iput(mip);
    return;
  }

  printf("Checking if %s already exists...\n", bname);
  if (search(mip, bname)) {
    printf("Error: Failed to create file at %s - %s already exists.\n", dname, bname);
    iput(mip);
    return;
  }

  kcreat(mip, bname, mode);
  mip->dirty = 1;
  iput(mip);
  printf("Created a new file\n\n");
}

void kcreat(MINODE *pmip, char *bname, int mode) {
  int ino = ialloc(dev);
  int blk = balloc(dev);

  MINODE *mip = iget(dev, ino);
  INODE *ip = &mip->INODE;
  ip->i_mode = mode;
  ip->i_block[0] = blk;
  for (int i = 1; i < 14; i++) {
    ip->i_block[i] = 0;
  }
  ip->i_links_count = 1;
  ip->i_uid = running->uid;
  ip->i_gid = running->gid;
  ip->i_size = 0;
  ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
  ip->i_blocks = 2;
  mip->dirty = 1; // Marked as dirty - node has been changed
  iput(mip);

  char buf[BLKSIZE];
  DIR *dp = (DIR *)buf;
  char *cp = buf;
  strcpy(dp->name, ".");
  dp->inode = ino;
  dp->rec_len = 12;
  dp->name_len = 1;

  // Move to next block and write ".."
  cp += dp->rec_len;
  dp = (DIR *)cp;
  strcpy(dp->name, "..");
  dp->inode = pmip->ino;
  dp->rec_len = BLKSIZE - 12; // make second block take up entire block length
  dp->name_len = 2;

  put_block(dev, blk, buf);
  enter_name(pmip, ino, bname);
}