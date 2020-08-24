#include "fs.h"

void create_copy(char* orig, char* dup){
    char buf[256];
    int fd1 = open_myfs(dup,'w');
    int fd2 = open_myfs(orig,'r');
    int i = 0;
    while(i = read_myfs(fd2,256,buf)){
        write_myfs(fd1,i,buf);
    }
    close_myfs(fd1);
    close_myfs(fd2);
}


int main(int argc, char** argv){

    int temp = 0;
    create_myfs(10);
    int fd1 = open_myfs((char *)"mytest.txt",'w');
    for(int i = 0; i < 100; i++){
        int random = rand()%1000;
        write_myfs(fd1,sizeof(int),(char *)&random);
    }
    close_myfs(fd1);

    cout << "Enter the number of copies to be made : ";
    cin >> temp;
    int i = 1;
    while(temp--){
        char temp_name[15];
        sprintf(temp_name,"mytest%d.txt",i++);
        create_copy((char *)"mytest.txt",temp_name);
    }
    ls_myfs();

    cout << "Dumping the file system into disk" << endl;
    dump_myfs((char *)"mydump-2.backup");
    
    shmdt(myfs);
    shmctl(shmid, IPC_RMID, 0);
}