#include "cli.h"
//判断是不是数字
int isNum(char *src,int len){
    int i;
    for (i = 0; i < len; i++) {
        if((src[i]<48)||(src[i]>57))
            return -1;
    }
    return 1;
}
//client端的read函数，同时对收到的信息进行判别
int read_msg_cli(int sockfd,char *dst,int read_size){
    //接收数据
    char buf[32];
    int n,i,len;
READ:
    n=read(sockfd,buf,read_size);
    if(n<0){
        if (errno==EINTR)
            goto READ;
        else{
            printf("n= %d ,read error: %s(errno: %d)\n",n,strerror(errno),errno);
            return -1;
        }//end of else
    }//end of if
    else if(n==0){
        printf("n= %d ,read error: %s(errno: %d)\n",n,strerror(errno),errno);
        return 0;
    }
    buf[n]='\0';
    //判别数据
    len=strlen(buf);
    if(len<=5){//如果传过来的是"StuNo"/"pid"/"TIME"/"end"
        if(strcmp(buf,dst)==0)
            return 1;
    }
    else if(len==8){//如果传过来的是"str*****"
        if(strncmp(buf,dst,3)==0){//前三个字符是str
            if(!(isNum(buf+3,5))){
                printf("client端收到strxxxxx格式有误。\n");
                return -1;
            }
            int ret=atoi(buf+3);
            if(ret>0)
                return ret;
            else{
            printf("client端收到str00000,有误。\n");
                return -1;
            }
        }
    }
    else{
        printf("client端收到的数据有误。将断开连接重新连接……\n");
        return -1;
    }
}

//循环write,确保写入全部数据
int write_msg_cli(int sockfd,char *buf,int write_size){
    int total=0;
    int n,sel;
    fd_set wfd;
    while(total < write_size){
        FD_ZERO(&wfd);
        FD_SET(sockfd,&wfd);

        sel=select(sockfd + 1, NULL, &wfd, NULL,NULL);
        if(sel <= 0){
            if (errno!=EINTR){
            printf("select wfd error: %s(errno: %d)\n", strerror(errno),errno);
            return -1;
            }
        }
        if (FD_ISSET(sockfd, &wfd)){
            FD_CLR(sockfd, &wfd);
WRITE:
            n=write(sockfd,buf+total,write_size-total);
            if(n<0){
                if (errno==EINTR)
                    goto WRITE;
                else{
                    printf("n= %d ,write error: %s(errno: %d)\n",n,strerror(errno),errno);
                    return -1;
                }//end of else
            }//end of if
            else if(n==0){
                printf("n= %d ,write error: %s(errno: %d)\n",n,strerror(errno),errno);
                return 0;
            }
            total+=n;
        }//end of if
    }//end of while
    return total;
}
//readn代替read,确保读到全部数据
int readn(int sockfd,char *buf,int len){
    int total=0;
    int n;
    while(total < len){
        n=read(sockfd,buf+total,len-total);
        if(n<=0){
            printf("n=%d,read error: %s(errno: %d)\n",n,strerror(errno),errno);
            return -1;
        }
        total+=n;
    }
    return total;
}
