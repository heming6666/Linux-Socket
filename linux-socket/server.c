#include <stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/ioctl.h>
#include<net/if.h>
#include <signal.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <sys/select.h>
#define MAXLINE 4096
#define MAXSIZE 1024

//������ʽ
int write_msg_cli(int sockfd,char *buf,int len);
int readn(int sockfd,char *buf,int len);
int myOpen(int pidNo);
//socketd ����\��\listen����
int get_socket_ser(char *bind_ip,int bind_port,int is_block){
    struct sockaddr_in servaddr;
    int sockfd;
    int n;
    //����socket������
    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
        return -1;
    }
    //�Ƿ���Ϊ������ģʽ
    if(!is_block){
        if ((n = fcntl(sockfd, F_GETFL, 0))<0) {
            printf("error: %s(errno: %d)\n",strerror(errno),errno);
            close(sockfd);
            return -1;
        }
        if (fcntl(sockfd, F_SETFL, n|O_NONBLOCK)<0) {
            printf("error: %s(errno: %d)\n",strerror(errno),errno);
            close(sockfd);
            return -1;
        }
    }
    //�ö˿�����
    int opt=1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0){
         printf("errno=%d(%s)\n", errno, strerror(errno));
         return -1;
     }
    //bind()��IP��ַ�Ͷ˿�
    bzero((char *)&servaddr, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(bind_port);
    if(strcmp(bind_ip,"0.0.0.0")==0){
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//������ָ��IP��ַ
    }
    else{
        inet_pton(AF_INET, bind_ip, &servaddr.sin_addr);
    }
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr))<0){
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    //����LISTEN״̬
    if (listen(sockfd, 1000)<0) {
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    return sockfd;
}

int msg_block(int sockfd) {
    int n;
    //StuNo
    int StuNo;
    int rcvStuNo;
    if(write(sockfd,"StuNo",5)<=0)
        return -1;
    if((n=read(sockfd,&rcvStuNo,4))!=4)
        return -1;
    StuNo=ntohl(rcvStuNo);
    //printf("n=%d,server�յ���%d\n",n,StuNo);

    //pid
    int pid;
    int rcvPid;
    if(write(sockfd,"pid",3)<=0)
        return -1;
    if((n=read(sockfd,&rcvPid,4))!=4)
        return -1;
    pid =ntohl(rcvPid);
    //printf("n=%d,server�յ���%d\n",n,pid);

    //TIME
    char time_str[20];
    if(write(sockfd,"TIME",5)==0)
        return -1;
    if((n=read(sockfd,time_str,19))<=0)
        return -1;
    time_str[n]='\0';
    //printf("server�յ���%s\n",time_str);
    //if((isRight(recvline,"TIME"))<0)
        //return -1;

    //str
    char rndStr[10];
    int rndNum;
    rndNum=rand()%100+1;
    sprintf(rndStr,"str%05d",rndNum);
    if(write(sockfd,rndStr,9)<=0)
        return -1;
    char *buf;
    buf=(char *)malloc(rndNum+1);
    if(buf==NULL) exit(1);
    if((n=readn(sockfd,buf,rndNum))<=0)
        return -1;
    buf[n]='\0';
    //printf("server�յ���%s\n",buf);
    //end
    char tmp[2];
    if(write(sockfd,"end",3)==0)
        return -1;
    if((n=read(sockfd,tmp,2))<=0){
        close(sockfd);
        //���ļ�
        int fp;
        if((fp=myOpen(pid))<0) return;
        char tmp[100];
        sprintf(tmp, "%d\n%d\n%s\n",StuNo,pid,time_str);
        write(fp,tmp,strlen(tmp));
        write_msg_cli(fp,buf,rndNum);
        close(fp);
        //�ͷ�����Ŀռ�
        if(buf!=NULL) free(buf);

        return 1;
    }
    return -1;
}
//��������ʽ
int msg_nonblock(int sockfd) {
    if(write(sockfd,"StuNo",5)<=0)
        return -1;
    int StuNo;
    int rcvStuNo;
    int pid;
    int rcvPid;
    char time_str[20];
    int rndNum;
    char *buf;
    int n;
    int stat=1;//״̬
    fd_set rfd;
    int sel,i;
    int maxfd=sockfd;
    while(1){
        FD_ZERO(&rfd);
        FD_SET(sockfd,&rfd);
        sel=select(maxfd + 1, &rfd, NULL, NULL, NULL);
        if(sel<0){
            printf("error: %s(errno: %d)\n",strerror(errno),errno);
            return -1;
        }
        if (FD_ISSET(sockfd, &rfd)){
            FD_CLR(sockfd, &rfd);
            if(stat==1){//��StuNo��pid
                if((n=read(sockfd,&rcvStuNo,4))!=4)
                    return -1;
                StuNo=ntohl(rcvStuNo);
                //printf("n=%d,server�յ���%d\n",n,StuNo);
                stat++;
                if(write(sockfd,"pid",3)<=0)
                    return -1;
            }
            else if(stat==2){//��pid��TIME
                if((n=read(sockfd,&rcvPid,4))!=4)
                    return -1;
                pid =ntohl(rcvPid);
                //printf("n=%d,server�յ���%d\n",n,pid);
                stat++;
                if(write(sockfd,"TIME",5)<=0)
                    return -1;
            }
            else if(stat==3){//��TIME��str
                if((n=read(sockfd,time_str,19))<=0)
                    return -1;
                time_str[n]='\0';
                //printf("server�յ���%s\n",time_str);
                stat++;
                char rndStr[10];
                rndNum=rand()%100+1;
                sprintf(rndStr,"str%05d",rndNum);
                if(write(sockfd,rndStr,9)<=0)
                    return -1;
            }
            else if(stat==4){//��str��end
                buf=(char *)malloc(rndNum+1);
                if(buf==NULL) exit(1);
                if((n=readn(sockfd,buf,rndNum))<=0)
                    return -1;
                buf[n]='\0';
                stat++;
                if(write(sockfd,"end",3)<=0)
                    return -1;
            }
            else if(stat==5){//end
                char tmp[2];
                if((n=read(sockfd,tmp,2))<=0){
                    close(sockfd);
                    //���ļ�
                    int fp;
                    if((fp=myOpen(pid))<0) return -1;
                    char tmp[100];
                    sprintf(tmp, "%d\n%d\n%s\n",StuNo,pid,time_str);
                    write(fp,tmp,strlen(tmp));
                    write_msg_cli(fp,buf,rndNum);
                    close(fp);
                    //�ͷ�����Ŀռ�
                    if(buf!=NULL) free(buf);
                    return 1;
                }
            }//end of else if(stat==5)
        }//if (FD_ISSET(sockfd, &rfd))
    }//end of while(1)
}
//-------------------fork��ʽ--------------------------
void isFork(char *bind_ip,int bind_port,int is_block) {
    int listenfd,connfd;
    pid_t pid;
    signal(SIGCHLD, SIG_IGN);//����SIGCHLD�ź�
    int clilen;
    struct sockaddr_in cliaddr;//,servaddr;
    int n;
    // ����socket
    listenfd = get_socket_ser(bind_ip,bind_port,is_block);
    if (listenfd < 0){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return;
    }
    printf("\n����fork��ʽ��socket�����󶨺��ˣ�������listen״̬��\n");
    while(1){
        clilen=sizeof(cliaddr);
        if(!is_block){//������
            fd_set rfd;
            int sel;
            int maxfd=listenfd;
            FD_ZERO(&rfd);
            FD_SET(listenfd,&rfd);
            sel=select(maxfd + 1, &rfd, NULL, NULL, NULL);
            if(sel<0){
                printf("error: %s(errno: %d)\n",strerror(errno),errno);
                return;
            }
            if (FD_ISSET(listenfd, &rfd)){
                FD_CLR(listenfd, &rfd);
                if( (connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen)) < 0){
                    printf("accept socket error: %s(errno: %d)\n",strerror(errno),errno);
                    break;
                }
                /* ��Ϊ������ģʽ */
                if ((n = fcntl(connfd, F_GETFL, 0))<0) {
                    printf("error: %s(errno: %d)\n",strerror(errno),errno);
                    break;
                }
                if (fcntl(connfd, F_SETFL, n|O_NONBLOCK)<0) {
                    printf("error: %s(errno: %d)\n",strerror(errno),errno);
                    break;
                }
            }
        }//end of if(!is_block)
        else{
            if( (connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen)) == -1){
                printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
                return;
            }
        }

        //��ӡclient �˵� IP ��ַ�Ͷ˿ں�
        char clientip[32];
        int cliport;
        inet_ntop(AF_INET,&cliaddr.sin_addr,clientip,sizeof(clientip));
        cliport=ntohs(cliaddr.sin_port);
        printf("\n�յ����Կͻ���IP��%s,PORT:%d�����ӣ�׼����������ȥ����\n",clientip,cliport);

        if((pid=fork())==0){//�ӽ���
            close(listenfd);
            int flag;
            if(is_block)
                flag=msg_block(connfd);
            else
                flag=msg_nonblock(connfd);
            if(flag>0)
                printf("\nserver�˵�һ��������������񣬹������ˡ�\n");
            else
                printf("\nserver�˵�һ��������;���㡭����\n");
            return;
        }
        close(connfd);
    }
}
//-------------------nonfork��ʽ--------------------------
void noFork(char *bind_ip,int bind_port) {
    struct sockaddr_in sin;
    int sockfd,connfd;
    int opt,flags,n;
    // ����socket
    sockfd = get_socket_ser(bind_ip,bind_port,0);
    if (sockfd < 0){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return;
    }
    printf("\n====== nonfork��ʽ,�ȴ��ͻ������ӡ���  ======\n");
    int cnt[5]={0,0,0,0,0};
    int i;
    int StuNo[MAXSIZE];
    int pid[MAXSIZE];
    char time_str[MAXSIZE][20];
    char *buf[MAXSIZE];
    int rcvStuNo;
    int rcvPid;
    int rndNum[MAXSIZE];
    int stat[MAXSIZE];//״̬
    for(i=0;i<MAXSIZE;i++)
        stat[i]=1;
    int client[MAXSIZE];
    int maxi = 0; // client��������󲻿���λ�õ��±�
    client[0] = sockfd;
    for (i = 1; i < MAXSIZE; i++)
        client[i] = -1;
    fd_set rfd;
    fd_set wfd;
    int sel;
    int maxfd=sockfd;
    while(1){
        FD_ZERO(&rfd);
        FD_ZERO(&wfd);
        FD_SET(sockfd,&rfd);
        for(i=1;i<=maxi;i++){
            connfd = client[i];
            if(connfd==-1)
                continue;
            FD_SET(connfd,&rfd);
            FD_SET(connfd,&wfd);
            if(connfd>maxfd)
                maxfd=connfd;
        }//for(i=0;i<=maxi;i++){

        sel=select(maxfd + 1, &rfd, NULL, NULL, NULL);

        for (i=0;i<=maxi;i++) {
            if(client[i] == -1)
                continue;
            if(i==0){//����listen��socket���⴦��
                if(sel<0){
                    if(errno==EINTR)
                        continue;
                    else{
                        printf("sockfd error: %s(errno: %d)\n",strerror(errno),errno);
                        return;
                    }
                }//if(sel<0)

                if (FD_ISSET(sockfd, &rfd)){
                    FD_CLR(sockfd, &rfd);
                    struct sockaddr_in cliaddr;
                    int len=sizeof(struct sockaddr_in);
        accept_again:
                    if( (connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &len)) < 0){
                        if(errno==EINTR)
                            goto accept_again;
                        else{
                            printf("accept socket error: %s(errno: %d)\n",strerror(errno),errno);
                            continue;
                        }
                    }//if( (connfd = accept

                    /* ��Ϊ������ģʽ */
                    if ((flags = fcntl(connfd, F_GETFL, 0))<0) {
                        printf("error: %s(errno: %d)\n",strerror(errno),errno);
                        continue;
                    }
                    if (fcntl(connfd, F_SETFL, flags|O_NONBLOCK)<0) {
                        printf("error: %s(errno: %d)\n",strerror(errno),errno);
                        continue;
                    }

                    for (i = 1; i < MAXSIZE; i++) {
                        if (client[i] < 0) {
                            client[i] = connfd;
                            if (i > maxi)
                                maxi = i;
                            break;
                        }
                    }
                    if (i == MAXSIZE) {
                        printf("too many clients!\n");
                        return;
                    }
                    printf("=====connfd = %d ���ӳɹ���======\n",connfd);
                    if(write(connfd,"StuNo",5)<=0){
                          printf("ֹ����write StuNo��connfd=%d���������ж�...\n",connfd);
                          close(connfd);
                          client[i]=-1;//ɾ���þ��
                          stat[i]=1;
                          if(buf[i]!=NULL) free(buf[i]);
                          continue;
                      }
                      printf("���͵�%d��StuNo��ȥ�ˡ�\n",++cnt[0]);
                    //printf("======connfd = %d�ɹ�����StuNo��======\n");
                }//if (FD_ISSET(sockfd, &rfd))
            }//if(i==0)
            else{
                connfd = client[i];
                if(sel < 0){
                    if (errno!=EINTR) {
                        printf("error: %s(errno: %d)\n",sel,strerror(errno),errno);
                        printf("ֹ����sel < 0��connfd=%d���������ж�...\n",connfd);
                        close(connfd);
                        continue;
                    }
                }
                if (FD_ISSET(connfd, &rfd)){
                    FD_CLR(connfd, &rfd);
                    if(stat[i]==1){//��StuNo��pid
                        if((n=read(connfd,&rcvStuNo,4))!=4){
                              printf("ֹ����read rcvStuNo��connfd=%d���������ж�...\n",connfd);
                              close(connfd);
                              client[i]=-1;//ɾ���þ��
                              stat[i]=1;
                              continue;
                          }
                        StuNo[i]=ntohl(rcvStuNo);
                        //printf("n=%d,server�յ���%d\n",n,StuNo[i]);
                        if(write(connfd,"pid",3)<=0){
                            printf("ֹ����write pid��connfd=%d���������ж�...\n",connfd);
                            close(connfd);
                            client[i]=-1;//ɾ���þ��
                            stat[i]=1;
                            continue;
                        }
                        stat[i]++;
                        printf("    ��%d��:��StuNo��pid��\n",++cnt[1]);
                    }
                    else if(stat[i]==2){//��pid��TIME
                        if((n=read(connfd,&rcvPid,4))!=4){
                            printf("ֹ����read rcvPid��connfd=%d���������ж�...\n",connfd);
                            close(connfd);
                            client[i]=-1;//ɾ���þ��
                            stat[i]=1;
                            continue;
                        }
                        pid[i] =ntohl(rcvPid);
                        //printf("n=%d,server�յ���%d\n",n,pid[i]);
                        stat[i]++;
                        if(write(connfd,"TIME",5)<=0){
                            printf("ֹ����write TIME��connfd=%d���������ж�...\n",connfd);
                            close(connfd);
                            client[i]=-1;//ɾ���þ��
                            stat[i]=1;
                            continue;
                        }
                        printf("         ��%d��:��pid��TIME��\n",++cnt[2]);
                    }
                    else if(stat[i]==3){//��TIME��str
                        if((n=read(connfd,time_str[i],19))<=0){
                            printf("ֹ����read time_str[i]��connfd=%d���������ж�...\n",connfd);
                            close(connfd);
                            client[i]=-1;//ɾ���þ��
                            stat[i]=1;
                            continue;
                        }
                        time_str[i][n]='\0';
                        stat[i]++;
                        char rndStr[10];
                        rndNum[i]=rand()%100+1;
                        sprintf(rndStr,"str%05d",rndNum[i]);
                        if(write(connfd,rndStr,9)<=0){
                            printf("ֹ����write rndStr��connfd=%d���������ж�...\n",connfd);
                            close(connfd);
                            client[i]=-1;//ɾ���þ��
                            stat[i]=1;
                            continue;
                        }
                        printf("                ��%d��:��TIME��str��\n",++cnt[3]);
                    }
                    else if(stat[i]==4){//��str��end
                        buf[i]=(char *)malloc(rndNum[i]+1);
                        if(buf[i]==NULL) exit(1);
                        if((n=readn(connfd,buf[i],rndNum[i]))<=0){
                            printf("ֹ����read buf[i]��connfd=%d���������ж�...\n",connfd);
                            close(connfd);
                            client[i]=-1;//ɾ���þ��
                            stat[i]=1;
                            if(buf[i]!=NULL) free(buf[i]);
                            continue;
                        }
                        buf[i][n]='\0';
                        stat[i]++;
                        if(write(connfd,"end",3)<=0){
                            printf("ֹ����write end��connfd=%d���������ж�...\n",connfd);
                            close(connfd);
                            client[i]=-1;//ɾ���þ��
                            stat[i]=1;
                            if(buf[i]!=NULL) free(buf[i]);
                            continue;
                        }
                        printf("                       ��%d��:��str��end��\n",++cnt[4]);
                    }
                    else if(stat[i]==5){//end
                        char tmp[2];
                        if((n=read(connfd,tmp,2))<=0){
                            close(connfd);
                            client[i]=-1;//ɾ���þ��
                            stat[i]=1;
                            //��д�ļ�
                            int fp;
                            if((fp=myOpen(pid[i]))<0) return;
                            char tmp[100];
                            sprintf(tmp, "%d\n%d\n%s\n",StuNo[i],pid[i],time_str[i]);
                            write(fp,tmp,strlen(tmp));
                            write_msg_cli(fp,buf[i],rndNum[i]);
                            close(fp);
                            //�ͷ�����Ŀռ�
                            if(buf[i]!=NULL) free(buf[i]);
                            //printf("connfd=%d��������д���ļ������ʹ��...\n",connfd);
                            printf("                             ��%d��:��end��д�ļ���\n",++cnt[5]);
                        }
                    }//end of else if(stat[i]==5)
                }//if (FD_ISSET(connfd, &rfd))
            }//else{
        }//for (i=0;i<=maxi;i++)
    }//while(1)
    close(sockfd);
    return ;
}
int main(int argc, char** argv)
{
    if (daemon(1, 1) == -1)
	{
		printf("daemon error!\n");
		return -1;
	}
    char bind_ip[16]="0.0.0.0";//ȱʡΪ"0.0.0.0"
    int bind_port;
    int port_flag=0;
    char tcp_or_udp[5]="tcp";//ȱʡtcp
    int is_fork=0;//ȱʡΪnonblock
    int is_block=0;//ȱʡΪnoFork
    int i;
    for(i=1;i<argc;i++){
        if(strcmp(argv[i],"-ip")==0){
            strcpy(bind_ip,argv[i+1]);
        }
        else if(strcmp(argv[i],"-port")==0){
            bind_port=atoi(argv[i+1]);
            port_flag=1;
        }
        else if(strcmp(argv[i],"-p")==0){
            strcpy(tcp_or_udp,argv[i+1]);
        }
        else if(strcmp(argv[i],"-block")==0){
            is_block=1;
        }
        else if(strcmp(argv[i],"-nonblock")==0){
            is_block=0;
        }
        else if(strcmp(argv[i],"-fork")==0){
            is_fork=1;
        }
        else if(strcmp(argv[i],"-nofork")==0){
            is_fork=0;
        }
    }
    //�ж��Ƿ�������port
    if(!port_flag){
        printf("��ʽ������ǵ�����Ҫ�󶨵Ķ˿ںš�\n");
        printf("�����˳��С���\n");
        return 0;
    }
    //noFork��blockͬʱ���֣�-block��Ч
    if(!is_fork)
        is_block=0;
    //���ú�������
    if(is_fork){
        isFork(bind_ip,bind_port,is_block);
    }
    else{
        noFork(bind_ip,bind_port);
    }
    return 0;
}
