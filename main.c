// CS 360 final project by Matt Balint and Evan Kimmerlein

#include "commands.h"

char gpath[128]; // global for tokenized components
char *name[64];  // assume at most 64 components in pathname
int n;           // number of component strings

int fd, dev;
int nblocks, ninodes, bmap, imap, iblk;
char line[128], cmd[32], pathname[128], pathname2[128];

MINODE minodes[NMINODE];
MINODE *root;
MOUNT mountTable[NMTABLE];
MOUNT *mp;
OFT oft[NOFT];
PROC procs[NPROC];
PROC *running;
SUPER *sp;
GD *gp;

int main(int argc, char *argv[]) {
  // Open diskimage
  char buf[BLKSIZE];
  char *disk;
  if (argc >= 1 && argv[1] != NULL) {
    disk = argv[1];
  } else {
    disk = "mydisk";
  }
  printf("Opening disk %s ...\n", disk);
  if ((fd = open(disk, O_RDWR)) < 0) {
    printf("Failed to open %s.\n", disk);
    exit(1);
  }

  dev = fd;
  mp = &mountTable[0];
  mp->dev = dev;

  // Read super block (block #1)
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;

  // Verify it's an EXT2 file system
  if (sp->s_magic != 0xEF53) {
    printf("%s is not an EXT2 File System. It has magic = %x\n", disk, sp->s_magic);
    exit(1);
  }
  printf("File system is EXT2\n");

  ninodes = mp->ninodes = sp->s_inodes_count;
  nblocks = mp->nblocks = sp->s_blocks_count;

  // Read group descriptors (block #2)
  get_block(dev, 2, buf);
  gp = (GD *)buf;

  bmap = mp->bmap = gp->bg_block_bitmap;
  imap = mp->imap = gp->bg_inode_bitmap;
  iblk = mp->nblocks = gp->bg_inode_table;
  printf("bmp=%d imap=%d inode_start=%d\n\n", bmap, imap, iblk);

  root = NULL;
  running = NULL;

  init();
  mount_root();

  int temp = 0;

  // Get user input
  while (1) {
    printf("------------------------\n\n");
    printf("[ls|cd|pwd|mkdir|rmdir|creat|link|unlink|symlink|readlink|chmod|utime|open|close|lseek|read|write|cat|cp|quit]\n");
    printf("input a command: ");
    fgets(line, 128, stdin);
    line[strlen(line) - 1] = 0;

    if (line[0] == 0) continue;

    pathname[0] = 0;
    pathname2[0] = 0;
    sscanf(line, "%s %s", cmd, pathname);
    printf("\n");

    if (!strcmp(cmd, "ls"))
      ls(pathname);
    else if (!strcmp(cmd, "cd")) {
      cd(pathname);
      pwd(running->cwd);
    } else if (!strcmp(cmd, "pwd"))
      pwd(running->cwd);
    else if (!strcmp(cmd, "quit"))
      quit(minodes);
    else if (!strcmp(cmd, "mkdir"))
      my_mkdir(pathname);
    else if (!strcmp(cmd, "rmdir"))
      my_rmdir(pathname);
    else if (!strcmp(cmd, "creat"))
      my_creat(pathname, 0100644);
    else if (!strcmp(cmd, "link")) {
      sscanf(line, "%s %s %s", cmd, pathname, pathname2);
      my_link(pathname, pathname2);
    } else if (!strcmp(cmd, "unlink"))
      my_unlink(pathname);
    else if (!strcmp(cmd, "symlink")) {
      sscanf(line, "%s %s %s", cmd, pathname, pathname2);
      my_symlink(pathname, pathname2);
    } else if (!strcmp(cmd, "readlink")) {
      my_readlink(pathname, buf);
      printf("%s\n\n", buf);
    } else if (!strcmp(cmd, "chmod")) {
      temp = 0;
      sscanf(line, "%s %d %s", cmd, &temp, pathname);
      my_chmod(temp, pathname);
    } else if (!strcmp(cmd, "utime"))
      my_utime(pathname);
    else if (!strcmp(cmd, "open")) {
      temp = -1;
      sscanf(line, "%s %s %d", cmd, pathname, &temp);
      my_open(pathname, temp);
    } else if (!strcmp(cmd, "close")) {
      temp = -1;
      sscanf(line, "%s %d", cmd, &temp);
      close_file(temp);
    } else if (!strcmp(cmd, "lseek")) {
      temp = -1;
      int position = 0;
      sscanf(line, "%s %d %d", cmd, &temp, &position);
      my_lseek(temp, position);
    } else if (!strcmp(cmd, "read")) {
      temp = -1;
      sscanf(line, "%s %d %s", cmd, &temp, pathname);
      my_read(temp, pathname, sizeof(pathname));
    } else if (!strcmp(cmd, "write")) {
      temp = -1;
      sscanf(line, "%s %d %s", cmd, &temp, pathname);
      my_write(temp, pathname, sizeof(pathname));
    } else if (!strcmp(cmd, "cat")) {
      my_cat(pathname);
    } else if (!strcmp(cmd, "cp")) {
      sscanf(line, "%s %s %s", cmd, pathname, pathname2);
      my_cp(pathname, pathname2);
    } else if (!strcmp(cmd, "mount")) {
      sscanf(line, "%s %s %s", cmd, pathname, pathname2);
      my_mount(pathname, pathname2);
    } else if (!strcmp(cmd, "unmount")) {
      my_unmount(pathname);
    }
  }

  return 0;
}