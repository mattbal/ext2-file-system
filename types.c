#include "types.h"
#include "util.h"

extern MINODE minodes[NMINODE];
extern MINODE *root;
extern PROC procs[NPROC];
extern PROC *running;
extern MOUNT mountTable[NMTABLE];
extern MOUNT *mp;
extern OFT oft[NOFT];
extern int dev;

void init() {
  int i = 0;
  MINODE *mip;
  PROC *p;
  MOUNT *mptr;

  printf("Initializing...\n");

  for (i = 0; i < NMINODE; i++) {
    mip = &minodes[i];
    mip->dev = mip->ino = 0;
    mip->refCount = 0;
    mip->mounted = 0;
    mip->mptr = 0;
  }

  for (i = 0; i < NMTABLE; i++) {
    mptr = &mountTable[i];
    mptr->dev = 0;
  }

  for (i = 0; i < NOFT; i++) {
    oft[i].refCount = 0;
  }

  for (i = 0; i < NPROC; i++) {
    p = &procs[i];
    p->status = FREE;
    p->pid = i;
    p->uid = i;
    p->gid = i;

    for (int j = 0; j < NFD; j++) {
      p->fd[j] = 0;
    }
    p->next = &procs[i + 1];
  }
  procs[NPROC - 1].next = &procs[0]; // create circular list

  printf("Initialization complete\n\n");
}

void mount_root() {
  printf("Mounting root\n");
  root = iget(dev, 2); // Mount root
  mp->mounted_inode = root;
  root->mptr = mp;
  printf("root refCount = %d\n\n", root->refCount);

  printf("Creating P0 as a running process\n");
  running = &procs[0];
  running->status = READY;
  running->cwd = iget(dev, 2);
  printf("root refCount = %d\n\n", root->refCount);

  printf("Creating P1 as a USER process\n");
  procs[1].status = READY;
  procs[1].cwd = iget(dev, 2);
  printf("root refCount = %d\n", root->refCount);

  printf("Done mounting root\n");
}

void quit() {
  MINODE *mip;
  for (int i = 0; i < NMINODE; i++) {
    mip = &minodes[i];
    if (mip->refCount && mip->dirty) {
      mip->refCount = 1;
      iput(mip);
    }
  }
  exit(0);
}