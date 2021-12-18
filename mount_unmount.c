#include "commands.h"

extern MOUNT mountTable[NMTABLE];
extern MOUNT *mp;
extern PROC *running;

void print_mounts() {
  printf("Mounted filesystems: \n");
  for (int i = 0; i < NMTABLE; i++) {
    if (mountTable[i].dev && mountTable[i].mounted_inode->mounted) {
      printf("dev: %d \tname: %s \tmount dir: %s\n", mountTable[i].dev, mountTable[i].name, mountTable[i].mount_name);
    }
  }
}

int my_mount(char *filesys, char *mount_point) {
  // If no parameters passed, print mounts
  if (!strcmp(filesys, "") && !strcmp(mount_point, "")) {
    print_mounts();
    return 0;
  }

  MOUNT *mptr;
  MINODE *mip;
  char buf[BLKSIZE];
  int fd = -1;

  // Check if it is already mounted
  for (int i = 0; i < NMTABLE; i++) {
    if (mountTable[i].dev && !strcmp(filesys, mountTable[i].name)) {
      printf("Filesystem already mounted!\n");
      printf("Rejected mount attempt.\n\n");
      return -1;
    }
  }

  // Find free entry
  int mountIndex = -1;
  for (int i = 0; i < NMTABLE; i++) {
    if (mountTable[i].dev == 0) {
      mountIndex = i;
      break;
    }
  }

  if (mountIndex == -1) {
    printf("Mount - Error: Max mounts reached.\n");
    return -1;
  }

  // Initialized mptr
  mptr = &mountTable[mountIndex];
  mptr->dev = 0;
  strcpy(mptr->name, filesys);
  strcpy(mptr->mount_name, mount_point);

  // Open filesystemm
  if ((fd = open(filesys, O_RDWR)) < 0) {
    printf("Mount - ERROR: failed to open file");
    return -1;
  }

  get_block(fd, 1, buf);
  SUPER *super = (SUPER *)buf;

  if (super->s_magic != 0xEF53) {
    printf("%s is not an EXT2 File System. It has magic = %x\n", filesys, super->s_magic);
    return -1;
  }

  int ino = getino(mount_point);
  mip = iget(running->cwd->dev, ino);

  // Check if it is a directory
  if (!S_ISDIR(mip->INODE.i_mode)) {
    printf("Mount - ERROR: %s is not a directory!", mount_point);
    return -1;
  }

  // Check if it is busy
  if (mip->refCount > 2) {
    printf("Mount - ERROR: Directory %s is busy", mount_point);
    return -1;
  }

  mptr->dev = fd;
  mptr->ninodes = super->s_inodes_count;
  mptr->nblocks = super->s_blocks_count;

  mip->mounted = 1;
  mip->mptr = mptr;
  mptr->mounted_inode = mip;

  printf("Mounted %s at %s.", filesys, mount_point);
}

int my_unmount(char *filesys) {
  return 0;
}
