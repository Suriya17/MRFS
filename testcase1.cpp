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

    /** 
     * Test Case 1 - deleting 5 files 
     */
    create_myfs(12);
    directory_block *root_dir = (directory_block*)(myfs + DATA_START);
    cout<<"Added 12 files into myfs\n";
    for(int i=0;i<12;i++)
    {
        char temp_name[15];
        sprintf(temp_name,"mycode%d.cpp",i);
        copy_pc2myfs((char *)"fs.cpp",temp_name);
    }

    copy_pc2myfs((char *)"Lab_5.pdf",(char *)"Lab_5.pdf");
    int opt;
    int fd,temp = 5;
    string s1,s2;
    
    while(temp--){
        ls_myfs();
        cout << "Enter the name of the file to be deleted : ";
        cin >> s1;
        rm_myfs((char *)s1.c_str());
    }

    /** 
     * Test Case 2 - deleting 5 files 
     */

    int fd1 = open_myfs((char *)"mytest.txt",'w');
    for(int i = 0; i < 100; i++){
        int random = rand();
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

    free(myfs);
}