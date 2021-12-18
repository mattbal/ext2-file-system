#include "commands.h"

extern int dev;
extern OFT oft[NOFT];
extern PROC *running;

int my_read(int fd, char *buf, int nbytes) {
  OFT *oftp = running->fd[fd];

  // Check if fd is opened for RD or RW
  if (oftp->mode != 0 && oftp->mode != 2) {
    printf("Read file - ERROR: File at fd %d is not opened for RD or RW mode!\n", fd);
    return -1;
  }

  MINODE *mip = oftp->mptr;
  int ibuf[256], ibuf2[256];
  int lbk, startByte, blk, remain;
  int count = 0; // number of bytes written
  int avil = mip->INODE.i_size - oftp->offset;

  char *cq = buf, *cp;

  while (nbytes && avil > 0) {
    lbk = oftp->offset / BLKSIZE;
    startByte = oftp->offset % BLKSIZE;

    if (lbk < 12) { // Direct blocks
      blk = mip->INODE.i_block[lbk];
    } else if (lbk >= 12 && lbk < 256 + 12) { // Indirect blocks
      get_block(mip->dev, mip->INODE.i_block[12], (char *)ibuf);
      blk = ibuf[lbk - 12];
    } else { // Double indirect blocks
      get_block(mip->dev, mip->INODE.i_block[13], (char *)ibuf);

      //Mailman's Algorithm
      lbk -= 256 - 12; // use 256 because int has 4 bytes and BLKSIZE is 1024
      blk = ibuf[lbk / 256];
      get_block(mip->dev, blk, (char *)ibuf2);
      blk = ibuf2[lbk % 256];
    }

    // read data from block
    char rbuf[BLKSIZE];
    get_block(mip->dev, blk, rbuf);

    // Copy to buf at most remain bytes
    cp = rbuf + startByte;
    remain = BLKSIZE - startByte;

    if (nbytes <= remain) {
      memcpy(cq, cp, nbytes);
      cq += nbytes;
      count += nbytes;
      oftp->offset += nbytes;
      avil -= nbytes;
      nbytes = 0;
    } else {
      memcpy(cq, cp, remain);
      cq += remain;
      count += remain;
      oftp->offset += remain;
      avil -= remain;
      nbytes -= remain;
    }
  }
  return count;
}

int my_cat(char *filename) {
  int fd = my_open(filename, 0);
  if (fd == -1) {
    printf("Error: Failed to open %s\n", filename);
    return -1;
  }

  int n;
  char buf[BLKSIZE];

  while (n = my_read(fd, buf, BLKSIZE)) {
    buf[n] = 0;
    char *c = buf;

    while (*c) {
      if (*c == '\n') {
        printf("\n");
      } else {
        printf("%c", *c);
      }
      c++;
    }
  }
  close_file(fd);
}