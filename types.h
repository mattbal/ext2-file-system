#ifndef types_h
#define types_h

#include <ext2fs/ext2_fs.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define FREE 0
#define READY 1

#define BLKSIZE 1024
#define NMINODE 128
#define NPROC 2

#define NMTABLE 8
#define NFD 10
#define NOFT 64

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct ext2_super_block SUPER;
typedef struct ext2_group_desc GD;
typedef struct ext2_inode INODE;
typedef struct ext2_dir_entry_2 DIR;

typedef struct minode {
  INODE INODE;
  int dev, ino; // (dev, ino) of INODE
  int refCount; // the number of users that are using the minode
  int dirty;    // 0 for clean, 1 for modified

  int mounted;        // for level-3
  struct Mount *mptr; // for level-3
} MINODE;

typedef struct oft {
  int mode;
  int refCount;
  MINODE *mptr;
  int offset;
} OFT;

typedef struct proc {
  struct proc *next;
  int pid;
  int ppid;
  int status;
  int uid, gid;
  MINODE *cwd;
  OFT *fd[NFD];
} PROC;

typedef struct Mount {
  int dev;     // dev (opened vdisk fd number) 0 means FREE
  int ninodes; // from superblock
  int nblocks;
  int bmap; // from GD block
  int imap;
  int iblk;
  struct minode *mounted_inode;
  char name[64];       // device name, e.g. mydisk
  char mount_name[64]; // mounted DIR pathname
} MOUNT;

void init();
void mount_root();
void quit();

#endif