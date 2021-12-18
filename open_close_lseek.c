#include "commands.h"

extern int fd, dev;
extern OFT oft[NOFT];
extern PROC *running;

// allocate a FREE OFT for use
OFT *oftalloc() {
  for (int i = 0; i < NOFT; i++) {
    OFT *o = &oft[i];
    if (o->refCount == 0) {
      o->refCount = 1;
      return o;
    }
  }
  printf("FS panic: out of OFTs\n");
  return 0;
}

// release a used OFT
void oftdalloc(OFT *o) {
  o->refCount = 0;
}

int my_truncate(MINODE *mip) {
  int ibuf[256];

  // Deallocate all direct data blocks in INODE
  for (int i = 0; i < 12; i++) {
    if (mip->INODE.i_block[i] == 0) break;
    bdalloc(mip->dev, mip->INODE.i_block[i]);
    mip->INODE.i_block[i] = 0;
  }

  // Deallocate all Indirect blocks on INODE
  if (mip->INODE.i_block[12]) {
    get_block(mip->dev, mip->INODE.i_block[12], (char *)ibuf);

    for (int i = 0; i < 256; i++) {
      if (ibuf[i]) {
        bdalloc(mip->dev, *ibuf);
        ibuf[i] = 0;
      }
    }
    mip->INODE.i_block[12] = 0;

    // Deallocate double indirect blocks
    if (mip->INODE.i_block[13]) {
      get_block(mip->dev, mip->INODE.i_block[13], (char *)ibuf);

      int ibuf2[256];

      // traverse 256 x 256 double indirect data blocks
      for (int i = 0; i < 256; i++) {
        get_block(mip->dev, mip->INODE.i_block[13], (char *)ibuf2);

        if (ibuf[i]) {
          for (int j = 0; j < 256; j++) {
            bdalloc(mip->dev, *ibuf2);
            ibuf2[i] = 0;
          }

          bdalloc(mip->dev, *ibuf);
          ibuf[i] = 0;
        }
      }

      mip->INODE.i_block[13] = 0;
    }
  }

  // Set size to 0, touch time and make dirty.
  mip->INODE.i_size = 0;
  mip->INODE.i_mtime = mip->INODE.i_atime = time(0L);
  mip->dirty = 1;
  iput(mip);

  return 1;
}

// flags is O_RDONLY, O_WRONLY, O_RDWR, which may be bitwise OR-ed with O_CREAT, O_APPEND, O_TRUNC, etc
// For simplicity, we shall assume the parameter flags = 0|1|2|3 or RD|WR|RW|AP for READ|WRITE|RDWR|APPEND
int my_open(char *filename, int flags) {
  // Check for valid mode
  if (flags < 0 || flags > 3) {
    printf("Open - Invalid mode: %d\n", flags);
    return -1;
  }

  int ino = getino(filename);
  if (!ino) {
    printf("File not found. attempting to create a new file...\n");
    my_creat(filename, 0100644);
    ino = getino(filename);
    // Check if creat failed
    if (!ino) {
      printf("Open - Invalid pathname: %s\n", filename);
      return -1;
    }
    printf("Created a new file\n");
  }

  printf("Path: %s\nMode: %d\n", filename, flags);

  MINODE *mip = iget(dev, ino);
  if (!mip) {
    printf("Error: Couldn't load inode %d\n\n", ino);
    return -1;
  }

  // Check for REGULAR file type
  if (!S_ISREG(mip->INODE.i_mode)) {
    printf("Error - %s is not a regular file\n", filename);
    iput(mip);
    return -1;
  }

  // Check for read permission
  if (!(mip->INODE.i_mode & S_IRUSR)) {
    iput(mip);
    printf("Error: Do not have read permission for file %s\n", filename);
    return -1;
  }

  OFT *o = oftalloc();
  if (!o) return -1;

  o->mode = flags;
  o->mptr = mip;

  // Set OFT offset based off of open mode (flag)
  switch (flags) {
  case 0: // Read
    o->offset = 0;
    break;
  case 1: // Write
    my_truncate(mip);
    o->offset = 0;
    break;
  case 2: // RW
    o->offset = 0;
    break;
  case 3: // Append
    o->offset = mip->INODE.i_size;
    break;
  default:
    printf("Error: Open - Invalid mode");
    return -1;
  }

  int i = 0;
  for (; i < NFD; i++) {
    if (running->fd[i] == 0) {
      running->fd[i] = o;
      break;
    }
  }

  if (i == NFD) {
    printf("Error: Failed to find open file descriptor\n");
    return -1;
  }

  mip->INODE.i_atime = time(0L);
  if (flags) mip->INODE.i_mtime = time(0L); // if W, RW or APPEND, update m_time
  mip->dirty = 1;

  return i; // returns a file descriptor
}

int close_file(int fd) {
  // Check if fd is within range
  if (fd < 0 || fd >= NFD) {
    printf("Close file - ERROR: fd of %d is out of range!\n", fd);
    return -1;
  }

  // Check if fd is a valid OFT entry
  if (!running->fd[fd]) {
    printf("Close file - ERROR: File at fd %d is not open!\n", fd);
    return -1;
  }

  OFT *oftp = running->fd[fd];
  running->fd[fd] = 0;
  oftp->refCount--;
  if (oftp->refCount > 0) return 0;

  // last user of OFT entry ==> dispose of the Minode[]
  MINODE *mip = oftp->mptr;
  iput(mip);

  return 0;
}

// for simplicity, we assume lseek always sets the offset
// position from the file beginning
int my_lseek(int fd, int position) {
  OFT *oftp = running->fd[fd];

  // Check that requested position is within bounds
  if (position < 0 || position > oftp->mptr->INODE.i_size - 1) {
    printf("Lseek - ERROR: position out of bounds!\n");
    return -1;
  }

  // Update offset
  int origionalPosition = oftp->offset;
  oftp->offset = position;

  return origionalPosition;
}