#include <iostream>
#include <bitset>
#include <ctime>
#include <cstdlib>
#include <sys/stat.h>

#define MAX_BLOCKS 131072
#define MAX_INODES 126
#define BLOCK_SIZE 256

using namespace std;

extern char* myfs;
extern int curr_inode;

struct data_block{
    char data[256];
};

struct super_block{
    int total_size;
    int max_inodes, used_inodes;
    int max_blocks, used_blocks;
    bitset<MAX_BLOCKS> bitmap;
    bitset<MAX_INODES> inode_bitmap;
};

struct inode{
    int file_type;
    mode_t st_mode;
    int file_size;
    int dir_blocks[8];
    int indir_block;
    int double_indir_block;
    time_t last_modified;
    time_t last_read;
};

int create_myfs(int size);
int copy_pc2myfs (char *source, char *dest);
int copy_myfs2pc (char *source, char *dest);
int rm_myfs (char *filename);
int showfile_myfs(char *filename);
int ls_myfs(char *filename);
int mkdir_myfs(char *dirname);
int chdir_myfs(char *dirname);
int rmdir_myfs(char *dirname);
int open_myfs(char *filename, char *mode);
int close_myfs(int fd);
int read_myfs(int fd, int nbytes, char *buf);
int write_myfs(int fd, int nbytes, char *buf);
int eof_myfs(int fd);
int dump_myfs(char *dumpfile);
int restore_myfs(char *dumpfile);
int status_myfs();
int chmod_myfs(char *name, int mode);