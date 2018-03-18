#include "fs.h"

int main(int argc, char** argv){
    create_myfs(12);
    open_myfs((char *)"mytest.txt",'w');

}