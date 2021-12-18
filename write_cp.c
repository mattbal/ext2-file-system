#include "commands.h"

extern int dev;
extern OFT oft[NOFT];
extern PROC *running;

int my_write(int fd, char buf[], int nbytes) {
  OFT *oftp = running->fd[fd];

  // Check if fd is a valid OFT entry
  if (!oftp) {
    printf("Write file - ERROR: File at fd %d is not open!\n", fd);
    return -1;
  }

  // Check if fd is opened with the correct mode
  if (oftp->mode == 0) {
    printf("Write file - ERROR: File at fd %d is opened for READ! It should be open for WR, RW, or APPEND mode.\n", fd);
    return -1;
  }

  MINODE *mip = oftp->mptr;
  char buf2[BLKSIZE];
  int ibuf[256] = {0};
  int lbk = 0;       // logical block
  int startByte = 0; // starting byte inside of lbk
  int blk, remain;
  int count = 0;

  char *cq = buf;
  char *cp;

  while (nbytes > 0) {
    lbk = oftp->offset / BLKSIZE;
    startByte = oftp->offset % BLKSIZE;

    if (lbk < 12) { // Direct blocks
      if (mip->INODE.i_block[lbk] == 0) {
        mip->INODE.i_block[lbk] = balloc(mip->dev); // must allocate a block
      }
      blk = mip->INODE.i_block[lbk];               // blk should be a disk block now
    } else if (lbk >= 12 && lbk < 256 + 12) {      // Indirect blocks
      if (mip->INODE.i_block[12] == 0) {           // indirect block hasn't been made yet
        mip->INODE.i_block[12] = balloc(mip->dev); // allocate a block

        // zero out block on disk
        memset(buf2, 0, BLKSIZE);
        put_block(mip->dev, mip->INODE.i_block[12], buf2);
        mip->INODE.i_blocks++; // increase # of blocks
      }

      // read data from block
      get_block(mip->dev, mip->INODE.i_block[12], (char *)ibuf);

      blk = ibuf[lbk - 12];
      if (blk == 0) { // can't find a block. need to create one
        ibuf[lbk - 12] = balloc(mip->dev);
        put_block(mip->dev, mip->INODE.i_block[12], (char *)ibuf);
        blk = ibuf[lbk - 12];
        mip->INODE.i_blocks++;
      }
    } else { // double indrect blocks
      if (mip->INODE.i_block[13] == 0) {
        mip->INODE.i_block[13] = balloc(mip->dev); // allocate a block

        // zero out block on disk
        memset(buf2, 0, BLKSIZE);
        put_block(mip->dev, mip->INODE.i_block[13], buf2);
        mip->INODE.i_blocks++;
      }

      get_block(mip->dev, mip->INODE.i_block[13], (char *)ibuf);
      lbk -= 256 - 12;
      int dblk = ibuf[lbk / 256]; // double indirect block

      if (dblk == 0) {
        ibuf[lbk / 256] = balloc(mip->dev); // allocate a block

        // zero out block on disk
        memset(buf2, 0, BLKSIZE);
        put_block(mip->dev, dblk, buf2);
        mip->INODE.i_blocks++;
        put_block(mip->dev, mip->INODE.i_block[13], (char *)ibuf);
      }

      get_block(mip->dev, dblk, (char *)ibuf);
      blk = ibuf[lbk % 256]; // second block

      if (blk == 0) {
        ibuf[lbk % 256] = balloc(mip->dev);
        put_block(mip->dev, dblk, (char *)ibuf);
        blk = ibuf[lbk % 256];
        mip->INODE.i_blocks++;
      }
    }

    // write to data block
    char wbuf[BLKSIZE];
    get_block(mip->dev, blk, wbuf);

    // Copy to wbuf at most remain bytes
    cp = wbuf + startByte;
    remain = BLKSIZE - startByte;

    if (remain > nbytes) {
      memcpy(cp, cq, nbytes);
      cq += nbytes;
      count += nbytes;
      nbytes = 0;
      oftp->offset += nbytes;
    } else {
      memcpy(cp, cq, remain);
      cq += remain;
      count += remain;
      nbytes -= remain;
      oftp->offset += remain;
    }

    // increase file size each time we write a big chunk of data
    if (oftp->offset > mip->INODE.i_size) {
      mip->INODE.i_size = oftp->offset;
    }

    put_block(mip->dev, blk, wbuf); // write block
  }

  mip->dirty = 1;
  printf("Wrote %d char into file descriptor fd=%d\n", count, fd);
  return count;
}

int my_cp(char *source, char *destination) {
  int fd = my_open(source, 0);
  if (fd == -1) {
    printf("Error: Failed to open %s\n", source);
    return -1;
  }

  int gd = my_open(destination, 1);
  if (gd == -1) {
    printf("Error: Failed to open %s\n", destination);
    close_file(fd);
    return -1;
  }

  int n;
  char buf[BLKSIZE];

  while (n = my_read(fd, buf, BLKSIZE)) {
    buf[n] = 0; // add null character to the end
    my_write(gd, buf, n);
    memset(buf, 0, BLKSIZE);
  }
  close_file(fd);
  close_file(gd);
}