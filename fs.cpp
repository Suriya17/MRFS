#include "fs.h"

char* myfs = NULL;
int curr_inode=0;
int myfs_size=0;
super_block *mySB ;
data_block *mydataspace;
fd_entry fd_table[MAX_FD_ENTRIES];

void init_indirect_block(int blk_num)
{
    indirect_pointers * idp = getidpaddr(blk_num);
    for(int i=0;i<64;i++)
    idp->arr[i]=-1;
}

int init_inode(inode *myinode){
    for(int i = 0; i < 8 ; i++)
        myinode->direct_blocks[i] = -1;
    myinode->indirect_block = -1;
    myinode->double_indirect_block = -1;
    myinode->file_size = 0;
    myinode->last_modified=time(0);
    myinode->last_read=time(0);
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

int getnextfreeinode(){
    for(int i = 0; i < mySB->max_inodes; i++){
        if(mySB->inode_bitmap[i] == 0){
            mySB->inode_bitmap[i] = 1;
            mySB->used_inodes++;
            return i;
        }
    }
    return -1;
}


int make_directory_entry(char *name, int file_inode,int entry_inode){

    inode * parent_inode=NULL;
    parent_inode = (inode *)(myfs + INODE_START+entry_inode*128);
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
                else if(strcmp(temp->folder[j].file_name,name)==0)
                    return -1;
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
    myfs_size=size_in_bytes;
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
    directory_block *root_dir = (directory_block*)(myfs + DATA_START);

    mydataspace = (data_block*)(myfs + DATA_START);

    init_directory_block(root_dir);
    strcpy(root_dir->folder[0].file_name,"root");
    root_dir->folder[0].inode_num = 0;
    mySB->bitmap.set(0,true);
    curr_inode = 0;
    fd_table_init();
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
    make_directory_entry(dest,free_inode_num,curr_inode);

    for(int i=0;i<min(8,(int)free_block_num.size());i++)
    {
        free_inode->direct_blocks[i]=free_block_num[i];
    }

    if(free_block_num.size()>8){
        int indirect_tmp = getnextfreeblock();
        init_indirect_block(indirect_tmp);
        indirect_pointers * idp = getidpaddr(indirect_tmp);
        free_inode->indirect_block = indirect_tmp;

        for(int i = 8; i < min(72,(int)free_block_num.size());i++){
            idp->arr[i-8] = free_block_num[i];
        }
    }


    if(free_block_num.size() > 72)
    {
        int indirect_tmp = getnextfreeblock();
        init_indirect_block(indirect_tmp);
        indirect_pointers * iddp = getidpaddr(indirect_tmp);
        indirect_pointers * idp;
        free_inode->double_indirect_block = indirect_tmp;
        // Check <= or <
        for(int i = 72; i < free_block_num.size();i++){
            if((i-72)%64 == 0){
                tmp = getnextfreeblock();
                init_indirect_block(tmp);
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
                cout<<setw(20)<<temp->folder[j].file_name<<" ";
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

            if(strcmp(filename,temp->folder[j].file_name)==0&&temp->folder[j].inode_num>=0)
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
  //  cout <<"copy file - " <<blocks_filled << endl;
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

int mkdir_myfs(char *dirname)
{
    int tmp=getnextfreeinode();
    if(tmp==-1) return -1;
    int temp=make_directory_entry(dirname,tmp,curr_inode);
    if(temp==-1) return -1;
    inode * new_dir_inode=getinodeaddr(tmp);
    init_inode(new_dir_inode);
    new_dir_inode->file_type = 0;
    new_dir_inode->st_mode=0644;
    make_directory_entry((char *)".",tmp,tmp);
    make_directory_entry((char *)"..",curr_inode,tmp);
    return 0;
}

int chdir_myfs(char *dirname)
{
    int tmp=search_fileinode(dirname,false);
    if(tmp==-1) return -1;
    inode *new_inode=getinodeaddr(tmp);
    if(new_inode->file_type==1)
    {
        cout<<"This is a file not directory\n";
        return -1;
    }
    curr_inode=tmp;
    return 0;
}

int rmdir_myfs(char *dirname)
{
    int curr_inode_temp=curr_inode;
    int tmp=search_fileinode(dirname,true);
    if(tmp==-1) {
        perror("Directory doesn't exist to delete");
        return -1;
    }
    inode *del_inode=getinodeaddr(tmp);
    for(int i=0;i<8;i++)
    {
        if(del_inode->direct_blocks[i]==-1) continue;
        directory_block *dir_entry=getdbaddr(del_inode->direct_blocks[i]);
        for(int k=0;k<8;k++)
        {
            if(dir_entry->folder[k].inode_num!=-1\
               &&(strcmp(dir_entry->folder[k].file_name,".")!=0)\
               &&(strcmp(dir_entry->folder[k].file_name,"..")!=0)\
               &&(strcmp(dir_entry->folder[k].file_name,"root")!=0))
            {
                    cout<<"deleting :"<<dir_entry->folder[k].file_name<<endl;
                    int inode_to_del=dir_entry->folder[k].inode_num;
                    inode *new_inode1=getinodeaddr(inode_to_del);
                    if(new_inode1->file_type==0)
                    {
                        curr_inode=tmp; 
                        rmdir_myfs(dir_entry->folder[k].file_name);
                        curr_inode=curr_inode_temp;
                    }
                    else{
                    curr_inode=tmp; 
                    rm_myfs(dir_entry->folder[k].file_name);
                    curr_inode=curr_inode_temp;
                    }
            }
        }
        if(mySB->bitmap[del_inode->direct_blocks[i]]==1) mySB->used_blocks--;
        mySB->bitmap[del_inode->direct_blocks[i]]=0;
    }
    mySB->inode_bitmap[tmp]=0;
    mySB->max_inodes--;
}

void fd_table_init()
{
    for(int i=0;i<MAX_FD_ENTRIES;i++)
        fd_table[i].file_inode_num=-1;
}

int getnextfreefdentry()
{
     for(int i=0;i<MAX_FD_ENTRIES;i++)
     {
        if(fd_table[i].file_inode_num==-1) 
        return i;
     }
    return -1;
}

int open_myfs(char *filename, char mode)
{
    int file_inodenum=search_fileinode(filename,false);
    if(file_inodenum==-1) return -1;
    int tmp=getnextfreefdentry();
    if(tmp==-1)
    {
        cout<<"No space left in fd_table\n";
        return -1;
    }
    fd_table[tmp].file_inode_num=file_inodenum;
    fd_table[tmp].bytes_done=0;
    fd_table[tmp].mode=mode;
    return tmp;
}

int close_myfs(int fd)
{
    if(fd>=MAX_FD_ENTRIES||(fd_table[fd].file_inode_num==-1))
        return -1;
    fd_table[fd].file_inode_num=-1;
    return 0;
}




int eof_myfs(int fd)
{
    if(fd>=MAX_FD_ENTRIES||(fd_table[fd].file_inode_num==-1)) 
        return -1;
    int file_inodenum=fd_table[fd].file_inode_num;
    int bytes_read=fd_table[fd].bytes_done;
    inode * file_inode=(inode *)getinodeaddr(file_inodenum);
    int f_size= file_inode->file_size;
    if(bytes_read==f_size)  return 1;
    else return 0;
}

int dump_myfs(char *dumpfile)
{
    int file_fd=open(dumpfile,O_WRONLY|O_CREAT,0664);
    if(file_fd < 0){
        cout << "OH NO FILE FD" << endl;
        return -1;
    }
    char buf[12];
    sprintf(buf,"%d",myfs_size);
    write(file_fd,buf,12);
    write(file_fd,myfs,myfs_size);
    close(file_fd);
    return 0;
}

int restore_myfs(char *dumpfile)
{
    int file_fd=open(dumpfile,O_RDONLY);
    if(file_fd < 0){
        cout << "OH NO FILE FD" << endl;
        return -1;
    }
    char buf[12];
    read(file_fd,buf,12);
    myfs_size=atoi(buf);
    myfs = (char*)malloc(myfs_size);
    read(file_fd,myfs,myfs_size);
    mySB = (super_block *) myfs;
    mydataspace = (data_block*)(myfs + DATA_START);
    curr_inode=0;
    close(file_fd);
    return 0;
}

int read_myfs(int fd, int nbytes, char *buf)
{
    int temp=nbytes;
    if(fd>=MAX_FD_ENTRIES||(fd_table[fd].file_inode_num==-1)||(fd_table[fd].mode=='w')) 
        return -1;
    int tmp=fd_table[fd].file_inode_num;
    int bytes_read=fd_table[fd].bytes_done;
    int start_block=bytes_read/BLOCK_SIZE;
    // cout << "Start block :" << start_block << endl;
    inode * file_inode=(inode *)getinodeaddr(tmp);
    int f_size= file_inode->file_size;
    if(bytes_read+nbytes>f_size)
    {
        nbytes=f_size-bytes_read;
        temp=nbytes;
    }
    int blocks_filled=getnumblocks(f_size);
    int tmp_write;
    int buffstart=0;
    for(int i=0;i<min(8,blocks_filled);i++)
    {
        if(nbytes==0) break;
        if(i==start_block)
        {
            int rem=BLOCK_SIZE-(bytes_read%BLOCK_SIZE);
            // cout << "Remaining :" << rem << endl;
            // cout << "nbytes :" << nbytes << endl; 
            memcpy(buf+buffstart,getdatablockaddr(file_inode->direct_blocks[i])->data+(bytes_read%BLOCK_SIZE),min(nbytes,rem));
            // cout<<"\n"<<i<<" th block :\n";
            //  myprint(buf+buffstart,min(nbytes,rem));
            buffstart+=(min(nbytes,rem));
            nbytes-=(min(nbytes,rem));
            // cout<<"Buff start1  :"<<buffstart<<endl;
        }
        else if(i>start_block)
        {
            memcpy(buf+buffstart,getdatablockaddr(file_inode->direct_blocks[i])->data,min(nbytes,BLOCK_SIZE));
            // cout<<"\n"<<i<<" th block yo :\n";
            // myprint(buf+buffstart,min(nbytes,BLOCK_SIZE));
            buffstart+=min(nbytes,BLOCK_SIZE);
            nbytes-=min(nbytes,BLOCK_SIZE);
            // cout<<"Buff start2  :"<<buffstart<<endl;
        }
       
    }
    
    if(blocks_filled > 8){
        
        indirect_pointers * idp = getidpaddr(file_inode->indirect_block);

        for(int i = 8; i < min(72,blocks_filled);i++){            
            if(nbytes==0) break;
            if(i==start_block)
            {
                int rem=BLOCK_SIZE-(bytes_read%BLOCK_SIZE);
                memcpy(buf+buffstart,getdatablockaddr(idp->arr[i-8])->data+(bytes_read%BLOCK_SIZE),min(nbytes,rem));
                //myprint(buf+buffstart,min(nbytes,rem));
                buffstart+=(min(nbytes,rem));
                nbytes-=(min(nbytes,rem));
            }
            else if(i>start_block)
            {
                memcpy(buf+buffstart,getdatablockaddr(idp->arr[i-8])->data,min(nbytes,BLOCK_SIZE));
                //myprint(buf+buffstart,min(nbytes,BLOCK_SIZE));
                buffstart+=min(nbytes,BLOCK_SIZE);
                nbytes-=min(nbytes,BLOCK_SIZE);
                
            }
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
            if(nbytes==0) break;
            if(i==start_block)
            {
                int rem=BLOCK_SIZE-(bytes_read%BLOCK_SIZE);
                memcpy(buf+buffstart,getdatablockaddr(idp->arr[(i-72)%64])->data+(bytes_read%BLOCK_SIZE),min(nbytes,rem));
                //  myprint(buf+buffstart,min(nbytes,rem));
                buffstart+=(min(nbytes,rem));
                nbytes-=(min(nbytes,rem));                
            }
            else if(i>start_block)
            {
                memcpy(buf+buffstart,getdatablockaddr(idp->arr[(i-72)%64])->data,min(nbytes,BLOCK_SIZE));
                // myprint(buf+buffstart,min(nbytes,BLOCK_SIZE));
                buffstart+=min(nbytes,BLOCK_SIZE);
                nbytes-=min(nbytes,BLOCK_SIZE);
            }
        }
    }
    fd_table[fd].bytes_done+=temp;
    
    return temp;
}



int write_myfs(int fd, int nbytes, char *buf)
{
    int temp=nbytes;
    if(fd>=MAX_FD_ENTRIES||(fd_table[fd].file_inode_num==-1)||(fd_table[fd].mode=='r')) 
        return -1;
    int tmp=fd_table[fd].file_inode_num;
    int bytes_read=fd_table[fd].bytes_done;
    int start_block=bytes_read/BLOCK_SIZE;
    inode * file_inode=(inode *)getinodeaddr(tmp);
    int f_size= file_inode->file_size;
    int buffstart=0;
    for(int i=0;i<8;i++)
    {
        if(nbytes==0) break;
        if(i==start_block)
        {
            int rem=BLOCK_SIZE-(bytes_read%BLOCK_SIZE);
            if(file_inode->direct_blocks[i]==-1)
            {
                file_inode->direct_blocks[i]=getnextfreeblock();
            }
            memcpy(getdatablockaddr(file_inode->direct_blocks[i])->data+(bytes_read%BLOCK_SIZE),buf+buffstart,min(nbytes,rem));
            buffstart+=(min(nbytes,rem));
            nbytes-=(min(nbytes,rem));
        }
        else if(i>start_block)
        {
            if(file_inode->direct_blocks[i]==-1)
                file_inode->direct_blocks[i]=getnextfreeblock();
            memcpy(getdatablockaddr(file_inode->direct_blocks[i])->data,buf+buffstart,min(nbytes,BLOCK_SIZE));
            buffstart+=min(nbytes,BLOCK_SIZE);
            nbytes-=min(nbytes,BLOCK_SIZE);
        }
    }
    if(nbytes>0){
    
        if(file_inode->indirect_block==-1) 
        {
            file_inode->indirect_block=getnextfreeblock();
            init_indirect_block(file_inode->indirect_block);
        }    
        indirect_pointers * idp = getidpaddr(file_inode->indirect_block);

        for(int i = 8; i < 72;i++){ 
            if(nbytes==0) break;
            if(i==start_block)
            {
                if(idp->arr[i-8]==-1)
                    idp->arr[i-8]=getnextfreeblock();
                int rem=BLOCK_SIZE-(bytes_read%BLOCK_SIZE);
                memcpy(getdatablockaddr(idp->arr[i-8])->data+(bytes_read%BLOCK_SIZE),buf+buffstart,min(nbytes,rem));
                buffstart+=(min(nbytes,rem));
                nbytes-=(min(nbytes,rem));
            }
            else if(i>start_block)
            {
                if(idp->arr[i-8]==-1)
                    idp->arr[i-8]=getnextfreeblock();
                memcpy(getdatablockaddr(idp->arr[i-8])->data,buf+buffstart,min(nbytes,BLOCK_SIZE));
                buffstart+=min(nbytes,BLOCK_SIZE);
                nbytes-=min(nbytes,BLOCK_SIZE);
                
            }
        }
    }
    if(nbytes>0){
        if(file_inode->double_indirect_block==-1) 
        {
            file_inode->double_indirect_block=getnextfreeblock();
            init_indirect_block(file_inode->double_indirect_block);
        }
        indirect_pointers * idp;
        indirect_pointers * iddp = getidpaddr(file_inode->double_indirect_block);
        for(int i = 72; i < 72+(64*64);i++){
            if(nbytes==0) break;
            if((i-72)%64 == 0){
                if(iddp->arr[(i-72)/64]==-1) 
                {
                    iddp->arr[(i-72)/64]=getnextfreeblock();
                    init_indirect_block(iddp->arr[(i-72)/64]);
                }
                idp = getidpaddr(iddp->arr[(i-72)/64]);
            }
            
            if(i==start_block)
            {
                if(idp->arr[(i-72)%64]==-1) idp->arr[(i-72)%64]=getnextfreeblock();
                int rem=BLOCK_SIZE-(bytes_read%BLOCK_SIZE);
                memcpy(getdatablockaddr(idp->arr[(i-72)%64])->data+(bytes_read%BLOCK_SIZE),buf+buffstart,min(nbytes,rem));
                buffstart+=(min(nbytes,rem));
                nbytes-=(min(nbytes,rem));                
            }
            else if(i>start_block)
            {
                if(idp->arr[(i-72)%64]==-1) idp->arr[(i-72)%64]=getnextfreeblock();
                memcpy(getdatablockaddr(idp->arr[(i-72)%64])->data,buf+buffstart,min(nbytes,BLOCK_SIZE));
                buffstart+=min(nbytes,BLOCK_SIZE);
                nbytes-=min(nbytes,BLOCK_SIZE);
            }
        }
    }
    
    fd_table[fd].bytes_done+=temp;
    file_inode->file_size=max(f_size,fd_table[fd].bytes_done);
    return temp;
}


int main()
{
    create_myfs(15);
    directory_block *root_dir = (directory_block*)(myfs + DATA_START);
    cout<<"Added 12 files into myfs\n";
    for(int i=0;i<12;i++)
    {
        char temp_name[15];
        sprintf(temp_name,"mycode%d.cpp",i);
        copy_pc2myfs((char *)"fs.cpp",temp_name);
    }
    copy_pc2myfs((char *)"Lab_5.pdf",(char *)"Lab_5.pdf");
    copy_pc2myfs((char *)"fs.cpp",(char *)"test.pdf");
    int opt;
    char buff[245];
    int fd,temp;
    string s1,s2;
   /* while(1)
    {
        cout<<"1. list all files\n";
        cout<<"2. Add a new directory\n";
        cout<<"3. Change Dir\n";
        cout<<"4. rm Dir\n";
        cout<<"5. rm File\n";
        cout<<"6. copy a new_file from PC to myfs\n";
        cout<<"7. copy a file from myfs to PC\n";
        cout<<"8. exit\n";
        cout<<"9. dumptofile\n";
        cout<<"10. restorefromfile\n";
        cout<<"select a option : ";
        cin>>opt;
        switch(opt)
        {
            case 1: ls_myfs();break;
            case 2: 
                cout<<"Enter dir name : ";
                cin>>s1;
                mkdir_myfs((char *)s1.c_str());
                break;
            case 3:
                cout<<"Enter dir name : ";
                cin>>s1;
                chdir_myfs((char *)s1.c_str());
                break;
            case 4:
                cout<<"Enter dir name : ";
                cin>>s1;
                rmdir_myfs((char *)s1.c_str());
                break;
            case 5:
                cout<<"Enter file name : ";
                cin>>s1;
                rm_myfs((char *)s1.c_str());
                break;
            case 6:
                cout<<"source file name :";
                cin>>s1;
                cout<<"dest file name :";
                cin>>s2;
                copy_pc2myfs((char *)s1.c_str(),(char *)s2.c_str());
                break;
            case 7:
                cout<<"source file name :";
                cin>>s1;
                cout<<"dest file name :";
                cin>>s2;
                copy_myfs2pc((char *)s1.c_str(),(char *)s2.c_str());
                break;
            case 8: 
                free(myfs);
                return 0;
            case 9:
                fd=open_myfs((char *)"mycode3.cpp",'r');
                
                read_myfs(fd,245,buff);
                myprint(buff,245);
                read_myfs(fd,245,buff);
                myprint(buff,245);
                break;
            case 10:
                restore_myfs((char *)"backup"); break;
            default:
                cout<<"Wrong option entered\n";
                break;
        }
    }*/
    ls_myfs();
    fd=open_myfs((char *)"Lab_5.pdf",'r');
    int fd1=open_myfs((char *)"test.pdf",'w');
    if(fd1==-1) cout<<"oh no fd\n";
    temp=read_myfs(fd,245,buff);
    write_myfs(fd1,temp,buff);
    // int sz=0;
    // sz+=temp;
    // cout << temp << endl;
    while(temp)
    {
        temp=read_myfs(fd,245,buff);
        write_myfs(fd1,temp,buff);
        // showfile_myfs((char *)"test.pdf");
        
        // cout<<"new read :" <<endl;
       // cout << temp << endl;
    }
    // cout<<sz<<endl;

    copy_myfs2pc((char *)"test.pdf",(char *)"test.pdf");
    close_myfs(fd);
    close_myfs(fd1);
}


