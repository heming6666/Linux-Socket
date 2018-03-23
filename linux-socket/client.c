#include "cli.h"
int write_msg_cli(int sockfd,char *buf,int write_size);
int read_msg_cli(int sockfd,char *dst,int read_size);
int myOpen(int pidNo);
//nofork方式在nonFork_cli.c里
int nonFork(char *remote_ip,int remote_port,int conn_num);
//获取系统时间
void getTime(char *time_str){
    time_t ptime;
    struct tm* p;
    time(&ptime);
    p=gmtime(&ptime);
    sprintf(time_str,"%d-%d-%d %d:%d:%d",(1900+p->tm_year),(1 + p->tm_mon),p->tm_mday,p->tm_hour,p->tm_min,p->tm_sec);
}
//创建socket描述符并设置模式（阻塞/非阻塞）
int create_socket(int is_block){
    int sockfd,n;
    //创建socket描述符
    while(1){
        if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
            return -1;
        }
        if(sockfd<3||sockfd>1023){
            close(sockfd);
            printf("socket=%d 不在3-1023范围\n",sockfd);
        }
        else break;
    }

    //是否设为非阻塞模式
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
    //置端口重用
    int opt=1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0){
         printf("errno=%d(%s)\n", errno, strerror(errno));
         return -1;
     }
     return sockfd;
}
//设置读写超时
int setTimeOut(int sockfd){
    struct timeval tv;
    tv.tv_sec=60;//60秒
    tv.tv_usec=0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)) < 0){
         printf("errno=%d(%s)\n", errno, strerror(errno));
         return -1;
     }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv)) < 0){
      printf("errno=%d(%s)\n", errno, strerror(errno));
      return -1;
    }
    return 1;
}
//连接socket
int conn_socket_cli(int sockfd,char *remote_ip,int remote_port,int is_block){
    struct sockaddr_in servaddr;
    //指定要连接的IP地址和端口
    bzero((char *)&servaddr, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(remote_port);
    inet_pton(AF_INET, remote_ip,&servaddr.sin_addr);
    //connect()连接
    if( connect(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) < 0){
        //非阻塞模式
        if(!is_block){
            fd_set wfd;
            int sel;
            int maxfd=sockfd;
            FD_ZERO(&wfd);
            FD_SET(sockfd,&wfd);
            struct timeval tm;
            tm.tv_sec = 10;
            tm.tv_usec = 0;
            sel=select(maxfd + 1, NULL, &wfd, NULL,&tm);
            if(sel<=0){
                if (errno!=EINTR){
                    printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
                    close(sockfd);
                    return -1;
                }
            }
        }
        else{
            printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
            close(sockfd);
            return -1;
        }
    }
}
//创建一个socket，并进行连接操作
int get_socket_cli(char *remote_ip,int remote_port,int is_block){
    int sockfd;
    //创建socket描述符
    if( (sockfd = create_socket(is_block)) < 0){
        return -1;
    }
    //阻塞方式设置读写超时
    if(is_block){
        if(setTimeOut(sockfd)<0)
            return -1;
    }
    //连接
    if( conn_socket_cli(sockfd,remote_ip,remote_port,is_block) < 0){
        return -1;
    }
    return sockfd;
}

//阻塞方式
int chld_block(char *remote_ip,int remote_port) {
    int flag=0;
    int sockfd;
    //struct sockaddr_in    servaddr;
    int n,i,sndNum;
    int StuNo=1552259;
    int pid=getpid();
    char time_str[20];
    int len;
    char *buf;
    int reconn_cnt=-1;
    while (1) {
        reconn_cnt++;
        if(reconn_cnt>10){
            printf(" -----pid= %d 重连了10次还没有？？？生气得退出了！------  \n",pid);
            return -1;
        }
        if(reconn_cnt>0){
            sleep(30);
            printf(" -----pid= %d 第 %d 次重连,...------  \n",pid,reconn_cnt);
        }

        //创建并连接
        if( (sockfd =get_socket_cli(remote_ip,remote_port,1)) < 0){
            return -1;
        }
        //StuNo
        sndNum=htonl(StuNo);
        if((read_msg_cli(sockfd,"StuNo",5))<0){
            printf(" pid= %d 倒在了Recv StuNo ... \n",pid);
            close(sockfd);
            continue;
        }
        if(write(sockfd,&sndNum,4)<=0){
            printf(" pid= %d 倒在了write StuNo ... \n",pid);
            close(sockfd);
            continue;
        }
        //pid
        sndNum=htonl(pid);
        if((read_msg_cli(sockfd,"pid",3))<0){
            printf(" pid= %d 倒在了Recv pid ... \n",pid);
            close(sockfd);
            continue;
        }
        if(write(sockfd,&sndNum,4)<=0){
            printf(" pid= %d 倒在了write pid ... \n",pid);
            close(sockfd);
            continue;
        }
        //TIME
        if((read_msg_cli(sockfd,"TIME",5))<0){
            printf(" pid= %d 倒在了Recv Time ... \n",pid);
            close(sockfd);
            continue;
        }
        getTime(time_str);
        if(write_msg_cli(sockfd,time_str,19)<=0){
            printf(" pid= %d 倒在了write time ... \n",pid);
            close(sockfd);
            continue;
        }
        //str
        len=read_msg_cli(sockfd,"str",9);
        if(len<=0){
            printf(" pid= %d 倒在了Recv Str ... \n",pid);
            close(sockfd);
            continue;
        }
        buf=(char *)malloc(len+1);
        if(buf==NULL) exit(1);
        for(i=0;i<len;i++)
            buf[i]=rand()%256;
        buf[len]='\0';
        if(write_msg_cli(sockfd,buf,len)<=0){
            printf(" pid= %d 倒在了write buf ... \n",pid);
            close(sockfd);
            continue;
        }
        //end
        if((read_msg_cli(sockfd,"end",3))<0){
            printf(" pid= %d 倒在了Recv end ... \n",pid);
            close(sockfd);
            continue;
        }
        close(sockfd);
        //写入文件
        int fp;
        if((fp=myOpen(pid))<0) return -1;
        char tmp[100];
        sprintf(tmp, "%d\n%d\n%s\n",StuNo,pid,time_str);
        write_msg_cli(fp,tmp,strlen(tmp));
        write_msg_cli(fp,buf,len);
        close(fp);
        //释放申请的空间
        if(buf!=NULL) free(buf);
        break;
    }
    return 1;
}
//非阻塞方式
int chld_nonblock(char *remote_ip,int remote_port) {
    int n,i,flag=0;
    int    sockfd;
    //struct sockaddr_in    servaddr;
    int StuNo=1552259;
    int pid=getpid();
    int sndNum;
    char time_str[20];
    char *buf;
    int len;
    int stat;
    fd_set rfd;
    int sel;
    int maxfd;
    struct timeval tm;
    int reconn_cnt=-1;
    while (1) {
        reconn_cnt++;
        if(reconn_cnt>10){
            printf(" -----pid= %d 重连了10次还没有？？？生气得退出了！------  \n",pid);
            return -1;
        }
        if(reconn_cnt>0){
            sleep(30);
            printf(" -----pid= %d 第 %d 次重连,...------  \n",pid,reconn_cnt);
        }

        //创建、绑定并连接
        if( (sockfd =get_socket_cli(remote_ip,remote_port,0)) < 0){
            return -1;
        }

        stat=1;//状态
        maxfd=sockfd;
        tm.tv_sec = 60;
        tm.tv_usec = 0;
        while(1){
            FD_ZERO(&rfd);
            FD_SET(sockfd,&rfd);
            sel=select(maxfd + 1, &rfd, NULL, NULL, &tm);
            if(sel==0){
                close(sockfd);
                break;
            }
            else if(sel<0){
                printf("error: %s(errno: %d)\n",strerror(errno),errno);
                printf(" pid= %d 倒在了sel ... \n",pid);
                close(sockfd);
                return -1;
            }
            if (FD_ISSET(sockfd, &rfd)){
                FD_CLR(sockfd, &rfd);
                if(stat==1){//StuNo
                    sndNum=htonl(StuNo);
                    if((read_msg_cli(sockfd,"StuNo",5))<0){
                        printf(" pid= %d 倒在了Recv StuNo ... \n",pid);
                        close(sockfd);
                        break;
                    }
                    if(write(sockfd,&sndNum,4)<=0){
                        printf(" pid= %d 倒在了write StuNo ... \n",pid);
                        close(sockfd);
                        break;
                    }
                    stat++;
                }
                else if(stat==2){//pid
                    sndNum=htonl(pid);
                    if((read_msg_cli(sockfd,"pid",3))<0){
                        printf(" pid= %d 倒在了Recv pid ... \n",pid);
                        close(sockfd);
                        break;
                    }
                    if(write(sockfd,&sndNum,4)<=0){
                        printf(" pid= %d 倒在了write pid ... \n",pid);
                        close(sockfd);
                        break;
                    }
                    stat++;
                }
                else if(stat==3){//TIME
                    if((read_msg_cli(sockfd,"TIME",5))<0){
                        printf(" pid= %d 倒在了Recv Time ... \n",pid);
                        close(sockfd);
                        break;
                    }
                    getTime(time_str);
                    if(write(sockfd,time_str,19)<=0){
                        printf(" pid= %d 倒在了write time ... \n",pid);
                        close(sockfd);
                        break;
                    }
                    stat++;
                }
                else if(stat==4){//str
                    len=read_msg_cli(sockfd,"str",9);
                    if(len<=0){
                        printf(" pid= %d 倒在了Recv Str ... \n",pid);
                        close(sockfd);
                        break;
                    }
                    buf=(char *)malloc(len+1);
                    if(buf==NULL) exit(1);
                    for(i=0;i<len;i++)
                        buf[i]=rand()%256;
                    buf[len]='\0';
                    if(write_msg_cli(sockfd,buf,len)<=0){
                        printf(" pid= %d 倒在了write buf ... \n",pid);
                        close(sockfd);
                        //释放申请的空间
                        if(buf!=NULL) free(buf);
                        break;
                    }
                    stat++;
                }
                else if(stat==5){//end
                    if((read_msg_cli(sockfd,"end",3))<0){
                        printf(" pid= %d 倒在了Recv end ... \n",pid);
                        close(sockfd);
                        break;
                    }
                    close(sockfd);
                    //写入文件
                    int fp;
                    if((fp=myOpen(pid))<0) return -1;
                    char tmp[100];
                    sprintf(tmp, "%d\n%d\n%s\n",StuNo,pid,time_str);
                    write_msg_cli(fp,tmp,strlen(tmp));
                    write_msg_cli(fp,buf,len);
                    close(fp);
                    //释放申请的空间
                    if(buf!=NULL) free(buf);
                    return 1;
                }
            }//if (FD_ISSET(sockfd, &rfd))
        }//end of while(1)
    }//end of while(1)
    return 1;
}
//子进程处理函数
int chldProc(char *remote_ip,int remote_port,int is_block) {
    pid_t pid=getpid();
    int flag;
    if(is_block)
        flag=chld_block(remote_ip,remote_port);
    else
        flag=chld_nonblock(remote_ip,remote_port);
    if(flag>0)
        return 1;
        //printf("\nclient端的pid=%d 儿子已完成任务，功成身退。\n",pid);
    else
        return -1;
        //printf("\nclient端的pid=%d 儿子中途崩殂……。\n",pid);
}
int RcyNum=0;
void Recycle_Handler()
{
    while(waitpid(-1,NULL,0)>0){
        RcyNum++;
        printf("已回收%d个子进程\n",RcyNum);
    }
}
//fork方式
void isFork(char *remote_ip,int remote_port,int fork_num,int is_block) {
    pid_t pid=1;
    int cnt=fork_num;
    signal(SIGCHLD, Recycle_Handler);
    while(1){
        if(pid<0){//分裂失败，退出循环
            break;
         }
         if (pid == 0){//子进程
            prctl(PR_SET_PDEATHSIG, SIGHUP);
            int con_cnt=0;
            while(chldProc(remote_ip,remote_port,is_block)<0){
                printf("chldProc 第 %d 次返回-1了！！！不气馁重连去！\n",++con_cnt);
                if(con_cnt>10){
                    printf("chldProc 已经第 10 次失败。\n");
                    break;
                }
            }
            return ;
         }
  	     else{//父进程
            if(cnt<=0)
                break;//循环次数到
            pid=fork();//分裂子进程
         }
         cnt--;
        }
    printf("client端已成功分裂出%d个子进程去连接server端去了。\n",fork_num-cnt);
    while (1) {
        sleep(2);
    }
}


int main(int argc, char** argv)
{
    if (daemon(1, 1) == -1)
	{
		printf("daemon error!\n");
		return -1;
	}
    char remote_ip[16];;
    int ip_flag=0;
    int remote_port;
    int port_flag=0;
    int conn_num=500;//缺省为500
    char tcp_or_udp[5]="tcp";//缺省tcp
    int is_fork=0;//缺省为nonblock
    int is_block=0;//缺省为noFork
    int i;
    for(i=1;i<argc;i++){
        if(strcmp(argv[i],"-ip")==0){
            strcpy(remote_ip,argv[i+1]);
            ip_flag=1;
        }
        else if(strcmp(argv[i],"-port")==0){
            remote_port=atoi(argv[i+1]);
            port_flag=1;
        }
        else if(strcmp(argv[i],"-num")==0){
            conn_num=atoi(argv[i+1]);
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
    //判断是否输入了ip和port
    if(!ip_flag){
        printf("格式有误，请记得输入要链接的服务端IP地址。\n");
        printf("程序退出中……\n");
        return 0;
    }
    if(!port_flag){
        printf("格式有误，请记得输入要链接的端口号。\n");
        printf("程序退出中……\n");
        return 0;
    }
    if(conn_num<1||conn_num>1000){
        printf("请输入1-1000的连接数。\n");
        printf("程序退出中……\n");
        return 0;
    }
    //noFork和block同时出现，-block无效
    if(!is_fork)
        is_block=0;

    //调用函数运行
    if(is_fork){
        isFork(remote_ip,remote_port,conn_num,is_block);
    }
    else{
        nonFork(remote_ip,remote_port,conn_num);
    }
    return 0;
}
