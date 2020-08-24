#include "fs.h"

void print_numbers(char *file){
    int fd = open_myfs(file,'r');
    int i = 0;
    int j;
    while(i = read_myfs(fd,sizeof(int),(char*)&j)){
        cout << j << " ";
    }
    cout << endl;
    close_myfs(fd);
}

void sortandstore(char *orig,char *sorted,int num){
    int arr[num];
    int fd = open_myfs(orig,'r');
    int i = read_myfs(fd,num*sizeof(int),(char*)arr);
    close_myfs(fd);
    sort(arr,arr+100);
    fd = open_myfs(sorted,'w');
    i = write_myfs(fd,num*sizeof(int),(char*)arr);
    close_myfs(fd);
}

int main(int argc, char** argv){
    cout << "Loading the file system back" << endl;

    restore_myfs((char *)"mydump-2.backup");
    ls_myfs();

    cout << "The contents of mytest.txt are : " << endl;
    print_numbers((char *)"mytest.txt");

    sortandstore((char *)"mytest.txt",(char *)"sorted.txt",100);
    cout << "The contents of sorted.txt are : " << endl;
    print_numbers((char *)"sorted.txt");


    shmdt(myfs);
    shmctl(shmid, IPC_RMID, 0);

}