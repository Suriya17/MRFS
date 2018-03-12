#include "fs.h"

char* myfs = NULL;

int create_myfs(int size){
    int size_in_bytes = size << 20;
    myfs = (char*)malloc(size_in_bytes);
    if(myfs == NULL)
        return -1;
    super_block *mySB = (super_block *) myfs;
    mySB->total_size = size;
    mySB->max_inodes = 126;
    mySB->max_blocks = size_in_bytes >> 8 - 128;
    mySB->used_inodes = 0;
    mySB->used_blocks = 0;
    mySB->bitmap.reset();
    mySB->inode_bitmap.reset();
    return 0;
}


int copy_pc2myfs (char *source, char *dest)
{
    
}


int main(){
    cout << sizeof(super_block) <<endl;
}