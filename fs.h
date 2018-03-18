#include <iostream>
#include <bitset>
#include <ctime>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cstdio>
#include <iomanip>
#include <vector>
#include <unistd.h>
#include <algorithm>

#define MAX_FD_ENTRIES 100
#define MAX_BLOCKS 131072
#define MAX_INODES 126
#define BLOCK_SIZE 256
#define INODE_START 16640
#define DATA_START 32768
#define DATA_DIR_START 128

using namespace std;

extern int myfs_size;
extern char* myfs;
extern int curr_inode;

struct data_block{
    char data[BLOCK_SIZE];
};

struct indirect_pointers{
    int arr[BLOCK_SIZE/4];
};

struct super_block{
    int total_size;
    int max_inodes, used_inodes;
    int max_blocks, used_blocks;
    bitset<MAX_BLOCKS> bitmap;
    bitset<MAX_INODES> inode_bitmap;
};

struct fd_entry{
    int file_inode_num;
    int bytes_done;
    char mode;
};


struct directory{
    char file_name[30];
    int16_t inode_num;
};

extern super_block *mySB ;
extern data_block *mydataspace;
extern fd_entry fd_table[MAX_FD_ENTRIES];

struct directory_block{
    directory folder[BLOCK_SIZE/32];
};

struct inode{
    int file_type;
    mode_t st_mode;
    int file_size;
    int direct_blocks[8];
    int indirect_block;
    int double_indirect_block;
    time_t last_modified;
    time_t last_read;
};

void fd_table_init();
void perm_print(mode_t mode);
int myprint(char *str,int size);
int getnextfreeblock();
int getnextfreeinode();
int getnextfreefdentry();
directory_block* getdbaddr(int i);
inode * getinodeaddr(int i);
indirect_pointers * getidpaddr(int i);
data_block *getdatablockaddr(int i);
int search_fileinode(char *filename, bool should_delete);
int make_directory_entry(char *name, int file_inode, int parent_inode);
int init_inode(inode *myinode);
int init_directory_block(directory_block *mydirblock);
void init_indirect_block(int blk_num);
void print_directory_block(directory_block *temp);
int decTooct(int i);

int create_myfs(int size);
int copy_pc2myfs (char *source, char *dest);
int copy_myfs2pc (char *source, char *dest);
int rm_myfs (char *filename);
int showfile_myfs(char *filename);
int ls_myfs();
int mkdir_myfs(char *dirname);
int chdir_myfs(char *dirname);
int rmdir_myfs(char *dirname);
int open_myfs(char *filename, char mode);
int close_myfs(int fd);
int read_myfs(int fd, int nbytes, char *buf);
int write_myfs(int fd, int nbytes, char *buf);
int eof_myfs(int fd);
int dump_myfs(char *dumpfile);
int restore_myfs(char *dumpfile);
int status_myfs();
int chmod_myfs(char *name, int mode);