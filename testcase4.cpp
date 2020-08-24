#include "fs.h"

int main(int argc, char** argv){
    create_myfs(10);
    mkdir_myfs((char *)"mycode");
    mkdir_myfs((char *)"mydocs");
    chdir_myfs((char *)"mydocs");
    mkdir_myfs((char *)"mytext");
    mkdir_myfs((char *)"mypapers");



    pid_t t = fork();
    // P1
    if(t == 0){
        chdir_myfs((char *)"mytext");
        int fd = open_myfs((char *)"alpha.txt",'w');
        char c = 'A';
        for(int i = 0; i < 26; i++){
            int j = write_myfs(fd,sizeof(char),(char *)&c);
            c++;
        }
        close_myfs(fd);
        ls_myfs();
        showfile_myfs((char *)"alpha.txt");
        cout << endl;
    }
    else{ // P2
        chdir_myfs((char *)"..");
        chdir_myfs((char *)"mycode");
        copy_pc2myfs((char *)"fs.cpp",(char*)"code.txt");
        ls_myfs();
    }

}