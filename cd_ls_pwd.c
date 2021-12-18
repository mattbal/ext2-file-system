#include "commands.h"

extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, iblk;
extern PROC *running;
extern MINODE *root;

void cd(char *pathname) {
  int ino = getino(pathname);
  if (!ino) {
    printf("Error: Failed to find an ino for directory %s\n\n", pathname);
    return;
  }

  MINODE *mip = iget(dev, ino);
  if (!mip) {
    printf("Error: Couldn't load inode %d\n\n", ino);
    return;
  }

  if (!S_ISDIR(mip->INODE.i_mode)) {
    printf("Error: %s is not a directory\n\n", pathname);
    iput(mip);
    return;
  }

  iput(running->cwd);
  running->cwd = mip;
}

void ls_file(MINODE *mip, char *name) {
  // print the file out as if -l was added
  if (S_ISREG(mip->INODE.i_mode)) {
    printf("-");
  } else if (S_ISDIR(mip->INODE.i_mode)) {
    printf("d");
  } else if (S_ISLNK(mip->INODE.i_mode)) {
    printf("l");
  }

  char *t1 = "xwrxwrxwr-------";
  char *t2 = "----------------";
  for (int i = 8; i >= 0; i--) {
    if (mip->INODE.i_mode & (1 << i)) {
      printf("%c", t1[i]); // print r|w|x
    } else {
      printf("%c", t2[i]); // print -
    }
  }
  printf("%4d ", mip->INODE.i_links_count);
  printf("%4d ", mip->INODE.i_gid);
  printf("%4d ", mip->INODE.i_uid);
  printf("%8d ", mip->INODE.i_size);

  char time[64];
  strcpy(time, ctime((const long int *)&mip->INODE.i_ctime));
  time[strlen(time) - 1] = 0;

  printf("%s ", time);
  printf("%s ", name);

  if (S_ISLNK(mip->INODE.i_mode)) {
    char buf[BLKSIZE];
    my_readlink(name, buf);
    printf("-> %s", buf);
  }
  printf("\n");
}

void ls_dir(MINODE *mip) {
  char buf[BLKSIZE], temp[256];

  get_block(dev, mip->INODE.i_block[0], buf);
  DIR *dp = (DIR *)buf;
  char *cp = buf;

  while (cp < buf + BLKSIZE) {
    strncpy(temp, dp->name, dp->name_len);
    temp[dp->name_len] = 0;

    MINODE *x = iget(dev, dp->inode);
    if (x) { // if not null
      ls_file(x, temp);
      iput(x);
    }

    cp += dp->rec_len;
    dp = (DIR *)cp;
  }
  printf("\n");
}

void ls(char *pathname) {
  int ino = getino(pathname);
  if (!ino) {
    printf("Error: Failed to find an ino for directory %s\n\n", pathname);
    return;
  }

  MINODE *mip = iget(dev, ino);

  if (!mip) {
    printf("Error: Couldn't load inode %d\n\n", ino);
    return;
  }

  if (S_ISDIR(mip->INODE.i_mode)) {
    ls_dir(mip);
  } else {
    printf("Error: %s is not a directory\n", pathname);
  }

  iput(mip);
}

void pwd(MINODE *wd) {
  if (wd == root) {
    printf("/\n\n");
  } else {
    rpwd(wd);
    printf("\n\n");
  }
}

void rpwd(MINODE *wd) {
  if (wd == root) return;

  //From wd->INODE.i_block[0] get parent_ino and my ino
  char buf[BLKSIZE];
  get_block(dev, wd->INODE.i_block[0], buf);
  int my_ino;
  int parent_ino = findino(wd, &my_ino);
  MINODE *pip = iget(dev, parent_ino);

  //from pip->INODE.i_block[] get my_name string by my_ino as LOCAL
  char my_name[256];
  findmyname(pip, my_ino, my_name);
  rpwd(pip);

  printf("/%s", my_name);
  iput(pip);
}
