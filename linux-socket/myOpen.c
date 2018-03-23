#include "cli.h"
//打开文件
int myOpen(int pidNo){

    char fname[32];
    //sprintf(fname,"mytxt/1552259.%d.pid.txt",pidNo);
    sprintf(fname,"1552259.%d.pid.txt",pidNo);
    int fp;
    if((fp=open(fname,O_WRONLY|O_CREAT,0777))<0){
        printf("打开文件%s失败!\n",fname);
        return -1;
    }
    //printf("成功打开文件%s!\n",fname);
    return fp;
}
