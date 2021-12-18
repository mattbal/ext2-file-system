rm a.out 2> /dev/null

mkdisk
gcc -m32 -g cd_ls_pwd.c link_unlink.c main.c misc.c mkdir_creat.c mount_unmount.c open_close_lseek.c read_cat.c rmdir.c symlink.c types.c util.c write_cp.c

# a.out
