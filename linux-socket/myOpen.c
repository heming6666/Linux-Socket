#include "cli.h"
//���ļ�
int myOpen(int pidNo){

    char fname[32];
    //sprintf(fname,"mytxt/1552259.%d.pid.txt",pidNo);
    sprintf(fname,"1552259.%d.pid.txt",pidNo);
    int fp;
    if((fp=open(fname,O_WRONLY|O_CREAT,0777))<0){
        printf("���ļ�%sʧ��!\n",fname);
        return -1;
    }
    //printf("�ɹ����ļ�%s!\n",fname);
    return fp;
}
