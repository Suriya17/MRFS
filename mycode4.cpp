#include "fs.h"

char* myfs = NULL;
int curr_inode=0;
super_block *mySB ;
data_block *mydataspace;



int init_inode(inode *myinode){
    for(int i = 0; i < 8 ; i++)
        myinode->direct_blocks[i] = -1;
    myinode->indirect_block = -1;
    myinode->double_indirect_block = -1;
    myinode->file_size = 0;
    return 0;
}

directory_block* getdbaddr(int i){
    return (directory_block*)(myfs + DATA_START + i*BLOCK_SIZE);
}

indirect_pointers * getidpaddr(int i){
    return (indirect_pointers *)(myfs + DATA_START + i*BLOCK_SIZE);
}

inode * getinodeaddr(int i){
    return (inode *)(myfs+INODE_START+i*(BLOCK_SIZE/2));
}

data_block *getdatablockaddr(int i){
    return (data_block *)(myfs + DATA_START + i*BLOCK_SIZE);
}

int init_directory_block(directory_block *mydirblock){
    for(int i = 0; i < 8; i++){
        mydirblock->folder[i].inode_num = -1;
    }
}

int getnextfreeblock(){
    for(int i = 0; i < mySB->max_blocks; i++){
        if(mySB->bitmap[i] == 0){
            mySB->bitmap[i] = 1;
            mySB->used_blocks++;
            return i;
        }
    }
    return -1;
}

int make_directory_entry(char *name, int file_inode){

    inode * parent_inode=NULL;
    parent_inode = (inode *)(myfs + INODE_START+curr_inode*128);
    directory_block *temp;
    parent_inode->file_size+=32;

    for(int i = 0; i < 8; i++){
        if(parent_inode->direct_blocks[i] >= 0){
            temp = getdbaddr(parent_inode->direct_blocks[i]);
            for(int j = 0; j < 8; j++){
                if(temp->folder[j].inode_num == -1){
                    temp->folder[j].inode_num = file_inode;
                    strcpy(temp->folder[j].file_name,name);
                    return 0;
                }
            }
        }
        else if(parent_inode->direct_blocks[i] == -1){
            parent_inode->direct_blocks[i] = getnextfreeblock();
            temp = getdbaddr(parent_inode->direct_blocks[i]);
            init_directory_block(temp);
            temp->folder[0].inode_num = file_inode;
            strcpy(temp->folder[0].file_name,name);
            return 0;
        }
    }
}

int create_myfs(int size){
    int size_in_bytes = size << 20;
    myfs = (char*)malloc(size_in_bytes);

    if(myfs == NULL)
        return -1;
    mySB = (super_block *) myfs;
    mySB->total_size = size;
    mySB->max_inodes = 126;
    mySB->max_blocks = (size_in_bytes >> 8) - 128;
    mySB->used_inodes = 1;
    mySB->used_blocks = 1;
    mySB->bitmap.reset();
    mySB->inode_bitmap.reset();
    
    mySB->inode_bitmap.set(0,true);

    inode *root_inode = (inode *)(myfs + INODE_START);
    init_inode(root_inode);
    root_inode->file_type = 0;
    root_inode->st_mode=0644;
    root_inode->direct_blocks[0] = 0;
    root_inode->last_modified=time(0);
    directory_block *root_dir = (directory_block*)(myfs + DATA_START);

    mydataspace = (data_block*)(myfs + DATA_START);

    init_directory_block(root_dir);
    strcpy(root_dir->folder[0].file_name,"root");
    root_dir->folder[0].inode_num = 0;
    mySB->bitmap.set(0,true);
    curr_inode = 0;

    return 0;
}


int copy_pc2myfs (char *source, char *dest){
    char buf[256];
    int filefd = open(source, O_RDONLY);
    int tmp;
    
    if(filefd < 0){
        perror("File doesn't exist");
        return -1;
    }

    struct stat filestat;
    fstat(filefd, &filestat);
	int filesize = filestat.st_size;
    mode_t perm = filestat.st_mode;
	close(filefd);

    int blocks_required = filesize/BLOCK_SIZE;

    if(filesize%BLOCK_SIZE > 0){
        blocks_required++;
    }

    
    if(mySB->max_blocks-mySB->used_blocks < blocks_required ) 
    {
        perror("No free space to store this file");
        return -1;
    }

    inode *free_inode = NULL;
    int free_inode_num=-1;
    for(int i=0;i<MAX_INODES;i++)
    {
        if(!mySB->inode_bitmap[i])
        {
            free_inode = (inode *)(myfs + INODE_START+i*128);
            free_inode_num=i;
            mySB->used_inodes++;
            mySB->inode_bitmap[i] = 1;
            break;
        }
    }

    if(free_inode==NULL) 
    {
        perror("File System full");
        return -1;
    }
    init_inode(free_inode);
    free_inode->file_type=1;
    free_inode->file_size=filesize;
    free_inode->st_mode=perm;
    // perm_print(perm);
    free_inode->last_modified=time(0);
    free_inode->last_read=time(0);

    int file_fd = open(source, O_RDONLY);

    data_block * free_data_block=NULL;
    vector<int> free_block_num;
    int p;
    for(int i=0;i< blocks_required;i++)
    {
            p = getnextfreeblock();
            free_data_block = getdatablockaddr(p);
            tmp = read(filefd,buf,BLOCK_SIZE);
            memcpy(free_data_block->data,buf,tmp);
            
            free_block_num.push_back(p);
            mySB->bitmap[p] = 1;
    }
    make_directory_entry(dest,free_inode_num);

    for(int i=0;i<min(8,(int)free_block_num.size());i++)
    {
        free_inode->direct_blocks[i]=free_block_num[i];
    }

    if(free_block_num.size()>8){
        int indirect_tmp = getnextfreeblock();
        indirect_pointers * idp = getidpaddr(indirect_tmp);
        free_inode->indirect_block = indirect_tmp;

        for(int i = 8; i < min(72,(int)free_block_num.size());i++){
            idp->arr[i-8] = free_block_num[i];
        }
    }


    if(free_block_num.size() > 72)
    {
        int indirect_tmp = getnextfreeblock();
        indirect_pointers * iddp = getidpaddr(indirect_tmp);
        indirect_pointers * idp;
        free_inode->double_indirect_block = indirect_tmp;
        // Check <= or <
        for(int i = 72; i < free_block_num.size();i++){
            if((i-72)%64 == 0){
                tmp = getnextfreeblock();
                iddp->arr[(i-72)/64] = tmp;
                idp = getidpaddr(tmp);
            }
            idp->arr[(i-72)%64] = free_block_num[i];
        }
    }
    close(file_fd);
}


void perm_print(mode_t mode)
{
    
    cout<< ((mode & S_IRUSR) ? "r" : "-");
    cout<< ((mode & S_IWUSR) ? "w" : "-");
    cout<< ((mode & S_IXUSR) ? "x" : "-");
    cout<< ((mode & S_IRGRP) ? "r" : "-");
    cout<< ((mode & S_IWGRP) ? "w" : "-");
    cout<< ((mode & S_IXGRP) ? "x" : "-");
    cout<< ((mode & S_IROTH) ? "r" : "-");
    cout<< ((mode & S_IWOTH) ? "w" : "-");
    cout<< ((mode & S_IXOTH) ? "x" : "-");
}

int ls_myfs()
{
    inode * parent_inode=NULL;
    parent_inode = (inode *)(myfs + INODE_START+curr_inode*128);
    inode *file_inode=NULL;
    for(int i=0;i<8;i++)
    {
       // cout<<"245o\n";
        int tmp=parent_inode->direct_blocks[i];
        if(tmp==-1) break;
        directory_block *temp=getdbaddr(tmp);
        for(int j=0;j<8;j++)
        {
            if(temp->folder[j].inode_num!=-1)
            {
                file_inode=getinodeaddr(temp->folder[j].inode_num);
                cout<< ((file_inode->file_type==0) ? "d" : "-");
                perm_print(file_inode->st_mode);
                cout<<" ";
                cout<<file_inode->file_size<<" ";
                cout<<setw(30)<<temp->folder[j].file_name<<" ";
                cout<<ctime(& (file_inode->last_modified));
            }
        }
    }
}

int search_fileinode(char *filename,bool should_delete){
    inode * parent_inode=NULL;
    parent_inode = (inode *)(myfs + INODE_START+curr_inode*128);
    inode *file_inode=NULL;
    
    for(int i=0;i<8;i++)
    {
        int tmp=parent_inode->direct_blocks[i];
        directory_block *temp=getdbaddr(tmp);
        for(int j=0;j<8;j++)
        {
            if(strcmp(filename,temp->folder[j].file_name)==0&&temp->folder[j].inode_num>0)
            {
                    tmp=temp->folder[j].inode_num;
                    if(should_delete)
                    {
                        temp->folder[j].inode_num=-1;
                        parent_inode->file_size-=32;
                    }
                    return tmp;
            }
        }

    }
    return -1;



}

int rm_myfs (char *filename){
    int tmp=search_fileinode(filename,true);
    if(tmp==-1)
    {
        perror("File doesn't exist to delete");
        return -1;
    }
    inode *file_inode = getinodeaddr(tmp);

    int filesize=file_inode->file_size;
    int blocks_occupied = filesize/BLOCK_SIZE;

    if(filesize%BLOCK_SIZE > 0){
        blocks_occupied++;
    }
    //cout<<"Block occupied  : "<<blocks_occupied<<endl;
    //deleting direct ptrs
    for(int i=0;i<8;i++)
    {
        if(blocks_occupied==0) break;
        //cout<<"removing "<<file_inode->direct_blocks[i]<<endl;
        if(mySB->bitmap[file_inode->direct_blocks[i]]==1)
            mySB->used_blocks--;
        mySB->bitmap[file_inode->direct_blocks[i]]=0;
        
        blocks_occupied--;
    }

    //deleting single indirect ptrs
    if(blocks_occupied>0){    
        indirect_pointers * idp = getidpaddr(file_inode->indirect_block);
        for(int i=0;i<64;i++)
        {
            if(blocks_occupied==0) break;
            if(mySB->bitmap[idp->arr[i]]==1)
            mySB->used_blocks--;
            //cout<<"removing "<<idp->arr[i]<<endl;
            mySB->bitmap[idp->arr[i]]=0;
            blocks_occupied--;
        }
        if(mySB->bitmap[file_inode->indirect_block]==1)
            mySB->used_blocks--;
        mySB->bitmap[file_inode->indirect_block]=0;
       // cout<<"removing "<<file_inode->indirect_block<<endl;
        
    }

    //deleting double indirect ptrs
    indirect_pointers * iddp = getidpaddr(file_inode->double_indirect_block);
    if(blocks_occupied>0){
        for(int i=0;i<64;i++)
        {
            if(blocks_occupied==0) break;
            indirect_pointers * idp = getidpaddr(iddp->arr[i]);
            for(int j=0;j<64;j++)
            {
                if(blocks_occupied==0) break;
                if(mySB->bitmap[idp->arr[j]]==1)
                    mySB->used_blocks--;
                mySB->bitmap[idp->arr[j]]=0;
                blocks_occupied--;
            }
            if(mySB->bitmap[iddp->arr[i]]==1)
                mySB->used_blocks--;
            mySB->bitmap[iddp->arr[i]]=0;
            
        }
        if(mySB->bitmap[file_inode->double_indirect_block]==1)
            mySB->used_blocks--;
        mySB->bitmap[file_inode->double_indirect_block]=0;
    }
    mySB->inode_bitmap[tmp]=0;
    mySB->used_inodes--;
    return 0;


}

int getnumblocks(int filesize)
{
    int blocks_occupied = filesize/BLOCK_SIZE;
    if(filesize%BLOCK_SIZE > 0){
        blocks_occupied++;
    }
    return blocks_occupied;
}

int myprint(char *str, int size){
    for(int i = 0; i < size; i++)
        cout << str[i];
}

int copy_myfs2pc (char *source, char *dest)
{
    int file_fd=open(dest,O_WRONLY|O_CREAT,0664);
    if(file_fd < 0){
        cout << "OH NO FILE FD" << endl;
    }
    int tmp=search_fileinode(source,false);
    inode * file_inode=(inode *)getinodeaddr(tmp);
    int f_size= file_inode->file_size;
    int blocks_filled=getnumblocks(f_size);
    cout <<"copy file - " <<blocks_filled << endl;
    int tmp_write;

    for(int i=0;i<min(8,blocks_filled);i++)
    {
        tmp_write = write(file_fd,getdatablockaddr(file_inode->direct_blocks[i])->data,min(256,f_size));
        //myprint(getdatablockaddr(file_inode->direct_blocks[i])->data);
        f_size -= tmp_write;
    }

    if(blocks_filled > 8){
        
        indirect_pointers * idp = getidpaddr(file_inode->indirect_block);

        for(int i = 8; i < min(72,blocks_filled);i++){
            tmp_write = write(file_fd,getdatablockaddr(idp->arr[i-8])->data,min(256,f_size));
            f_size -= tmp_write;
        }
    }

    if(blocks_filled > 72)
    {
        indirect_pointers * iddp = getidpaddr(file_inode->double_indirect_block);
        indirect_pointers * idp;
        for(int i = 72; i < blocks_filled;i++){
            if((i-72)%64 == 0){
                idp = getidpaddr(iddp->arr[(i-72)/64]);
            }
            tmp_write = write(file_fd,getdatablockaddr(idp->arr[(i-72)%64])->data,min(256,f_size));
            f_size -= tmp_write;
        }
    }

    close(file_fd);

    return f_size;
}


int showfile_myfs (char *filename)
{
    int tmp=search_fileinode(filename,false);
    inode * file_inode=(inode *)getinodeaddr(tmp);
    int f_size= file_inode->file_size;
    int blocks_filled=getnumblocks(f_size);

    for(int i=0;i<min(8,blocks_filled);i++)
    {
        myprint(getdatablockaddr(file_inode->direct_blocks[i])->data,min(256,f_size));
        f_size -= min(256,f_size);
    }

    if(blocks_filled > 8){
        
        indirect_pointers * idp = getidpaddr(file_inode->indirect_block);

        for(int i = 8; i < min(72,blocks_filled);i++){
            myprint(getdatablockaddr(idp->arr[i-8])->data,min(256,f_size));
            f_size -= min(256,f_size);
        }
    }

    if(blocks_filled > 72)
    {
        indirect_pointers * iddp = getidpaddr(file_inode->double_indirect_block);
        indirect_pointers * idp;
        for(int i = 72; i < blocks_filled;i++){
            if((i-72)%64 == 0){
                idp = getidpaddr(iddp->arr[(i-72)/64]);
            }
            myprint(getdatablockaddr(idp->arr[(i-72)%64])->data,min(256,f_size));
            f_size -= min(256,f_size);
        }
    }

    return f_size;
}

int main(){
    create_myfs(10);
    directory_block *root_dir = (directory_block*)(myfs + DATA_START);
    copy_pc2myfs((char *)"fs.cpp",(char *)"mycode.cpp");
    cout<<"Deleting mycode.cpp \n";
    rm_myfs((char *)"mycode.cpp");
    copy_pc2myfs((char *)"fs.cpp",(char *)"mycode.cpp");
    copy_pc2myfs((char *)"fs.h",(char *)"mycode2.cpp");
    copy_myfs2pc((char *)"mycode.cpp",(char *)"mycode4.cpp");
    ls_myfs();
    
    cout<<"Deleting mycode.cpp \n";
    rm_myfs((char *)"mycode.cpp");
    ls_myfs();
    cout<<"Showing file fs.h"<<endl;
    showfile_myfs((char *)"mycode2.cpp");
    cout<<"\nDeleting mycode2.cpp \n";
    rm_myfs((char *)"mycode2.cpp");
    ls_myfs();
    cout<<"\nNo of blocks used : "<<mySB->used_blocks<<endl;
    free(myfs);
}p");
    copy_myfs2pc((char *)"mycode.cpp",(char *)"mycode4.cpp");
    ls_myfs();
    
    cout<<"Deleting mycode.cpp \n";
    rm_myfs((char *)"mycode.cpp");
    ls_myfs();
    cout<<"Showing file fs.h"<<endl;
    showfile_myfs((char *)"mycode2.cpp");
    cout<<"\nDeleting mycode2.cpp \n";
    rm_myfs((char *)"mycode2.cpp");
    ls_myfs();
    cout<<"\nNo of blocks used : "<<mySB->used_blocks<<endl;
}used_blocks<<endl;
}