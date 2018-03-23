#include "cli.h"
int time_flag=0;//已触发定时器次数
void alarm_handler(){
    time_flag++;
    alarm(TIMER_PIECE);
}
//nofork方式
int nonFork(char *remote_ip,int remote_port,int conn_num){
    int *fd_arr;//用于存放每次申请到的fd
    fd_arr=(int*)malloc(sizeof(int)*conn_num);
    int fd_cnt=0;//每次创建的fd数目

    int *tm;//用于存放每个fd等待时间
    tm=(int *)malloc(sizeof(int)*conn_num);

    int *stat;//表示每个连接的状态
    stat=(int *)malloc(sizeof(int)*conn_num);

    msg_cli *msg;//用于存放收发数据
    msg=(msg_cli *)malloc(sizeof(msg_cli)*conn_num);

    int network_order_msg_temp;//将本机序转化为网络序时临时变量
    int fresh_fd_cnt=0;//记录已经申请并连接了多少个fd
    int fail_fd_cnt=0;//记录总计多少个fd连接失败的
    int completed_fd_num=0;//已完成收发数据的sockfd编号
    int *stat_cnt;
    stat_cnt=(int *)malloc(sizeof(int)*STAT_CNT);
    char log_time[20];
    int i,j,n;
    fd_set rfd;
    int maxfd;
    int sel;
    pid_t pid=getpid();

    int rfd_cnt=0;

    //初始化工作
    for(i=0;i<conn_num;i++){
        fd_arr[i]=-1;
        tm[i]=0;
        stat[i]=INIT;
        msg[i].stuno=1552259;
        msg[i].pid=(pid<<16)+i;
    }

    // 定时器
    signal(SIGALRM, alarm_handler);
    alarm(TIMER_PIECE);
    while (1) {
        //连接全部完成
        if(completed_fd_num>=conn_num)
            break;
        //统计状态的数组清0初始化
        for(i=0;i<STAT_CNT;i++){
            stat_cnt[i]=0;
        }
        //计时器处理部分
        if(time_flag>0){
            for(i=0;i<conn_num;i++){
                if((stat[i]!=INIT)&&(stat[i]!=COMPLETED)){//如果还没申请fd/已经完事的则不用计时
                    tm[i]+=time_flag;//累计
                    //如果已经超时，有两种情况：
                    //1、超时没反应，马上重新连接
                    //2、由于某些原因连接中断返回-1，已经等了足够长时间，可以重新连接了
                    if(tm[i]>=RETRY_TIME){
                        if(stat[i]<COMPLETED){//情况1,需要关闭连接
                            close(fd_arr[i]);
                            fd_arr[i]=-1;
                            fail_fd_cnt++;
                        }//end of if
                        stat[i]=INIT;//设置为可以重新连接的状态
                        tm[i]=0;//计时器清零
                    }//end of if
                }//end of if
                if(stat[i]<COMPLETED){
                    stat_cnt[stat[i]]++;//统计处于各个状态fd个数
                }
            }//end of for
            printf("共发起连接:[%d/%d],已成功:[%d],共失败:[%d]\n",fresh_fd_cnt,conn_num,completed_fd_num,fail_fd_cnt);
            //打印统计信息
            for(i=0;i<STAT_CNT;i++){
                if(stat_cnt[i]>0)
                    printf("[stat=%d] : %d个 \n",i, stat_cnt[i]);
            }
            printf("--------\n");
        }//end of if
        time_flag=0;

        //申请socket，一次只申请指定数目个
        fd_cnt=0;
        for(i=0;i<conn_num;i++){
            if(stat[i]==INIT){//处于初始状态，可以申请socket
                //申请socket
                if((fd_arr[i]=get_socket_cli(remote_ip,remote_port,NON_BLOCK))< 0){
                    return -1;
                };
                fresh_fd_cnt++;
                stat[i]=READ_STUNO;//修改状态
                fd_cnt++;//本次已经创建了多少个
                if(fd_cnt>FD_CNT_PER_TIME)
                    break;
            }//end of if
        }//end of for

        FD_ZERO(&rfd);
        maxfd=0;
        for(i=0;i<conn_num;i++){
            //等待收发数据状态
            if((stat[i]>=READ_STUNO)&&(stat[i]<=READ_END)){
                FD_SET(fd_arr[i],&rfd);//置读集
            }
            //更新maxfd
            if(fd_arr[i]>maxfd)
                maxfd=fd_arr[i];
        }//end of for
        sel=select(maxfd + 1, &rfd, NULL, NULL,NULL);
        //select返回<=0，如不是中断引起则出错返回，否则continue继续while循环
        if(sel<=0){
            if (errno!=EINTR){
                printf("select error: %s(errno: %d)\n", strerror(errno),errno);
                //把打开的都关了，申请的都释放了
                for(i=0;i<conn_num;i++){
                    if(fd_arr[i]>0)
                        close(fd_arr[i]);
                    if(msg[i].buf!=NULL)
                        free(msg[i].buf);
                }//end of for
                return -2;//错误返回
            }//end of if
            else
                continue;
        }//end of if

        //逐一检查读写集
        for(i=0;i<conn_num;i++){
            //还没申请socket的直接跳过
            if(fd_arr[i]<0)
                continue;
            //rfd有效，可以收发数据
            if (FD_ISSET(fd_arr[i], &rfd)){
                FD_CLR(fd_arr[i], &rfd);
                //printf("fd=%d,我也不知道为啥来了\n", fd_arr[i]);
                //printf("stat[i]=%d,我也不知道为啥来了\n", stat[i]);
                if(stat[i]==READ_STUNO){//READ_STUNO
                    n=read_msg_cli(fd_arr[i],STUNO,STUNO_READ_LEN);
                    if(n<=0){
                        printf("[fd=%d]连接关闭,stat=READ_STUNO \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        //如果返回-1，read出错，置ERR_READ,需等待一段时间再重连
                        //如果返回0，对方关闭连接，可以直接重新连接
                        stat[i]=ERR_READ;
                        continue;
                    }//end of if
                    stat[i]=WRITE_STUNO;//更新状态
                    //先把主机序转化为网络序
                    network_order_msg_temp=htonl(msg[i].stuno);
                    n=write_msg_cli(fd_arr[i],(char *)&network_order_msg_temp,STUNO_WRITE_LEN);
                    if(n<=0){
                        printf("[fd=%d]连接关闭,stat=WRITE_STUNO \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_WRITE;
                        continue;
                    }
                    tm[i]=0;//计时器清零
                    stat[i]=READ_PID;//更新状态
                }//end of StuNo
                else if(stat[i]==READ_PID){//READ_PID
                    n=read_msg_cli(fd_arr[i],PID,PID_READ_LEN);
                    if(n<=0){
                        printf("[fd=%d]连接关闭,stat=READ_PID \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_READ;
                        continue;
                    }//end of if
                    stat[i]=WRITE_STUNO;//更新状态
                    //先把主机序转化为网络序
                    network_order_msg_temp=htonl(msg[i].pid);
                    n=write_msg_cli(fd_arr[i],(char *)&network_order_msg_temp,PID_WRITE_LEN);
                    if(n<=0){
                        printf("[fd=%d]连接关闭,stat=WRITE_PID \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_WRITE;

                        continue;
                    }
                    tm[i]=0;//计时器清零
                    stat[i]=READ_TIME;//更新状态
                }//end of PID
                else if(stat[i]==READ_TIME){//READ_TIME
                    n=read_msg_cli(fd_arr[i],TIME,TIME_READ_LEN);
                    if(n<=0){
                        printf("[fd=%d]连接关闭,stat=READ_TIME \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_READ;
                        continue;
                    }//end of if
                    stat[i]=WRITE_TIME;//更新状态
                    getTime(msg[i].time_str);
                    n=write_msg_cli(fd_arr[i],msg[i].time_str,TIME_WRITE_LEN);
                    if(n<=0){
                        printf("[fd=%d]连接关闭,stat=WRITE_TIME \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_WRITE;
                        continue;
                    }
                    tm[i]=0;//计时器清零
                    stat[i]=READ_STR;//更新状态
                }//end of TIME
                else if(stat[i]==READ_STR){//READ_STR
                    msg[i].str_len=read_msg_cli(fd_arr[i],STR,STR_READ_LEN);
                    if(n<=0){
                        printf("[fd=%d]连接关闭,stat=READ_STR \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        //if(n<0)
                            stat[i]=ERR_READ;
                        //else
                            //stat[i]=INIT
                        continue;
                    }//end of if
                    stat[i]=WRITE_STR;//更新状态
                    msg[i].buf=(char *)malloc(msg[i].str_len);
                    if(msg[i].buf==NULL){
                        //把打开的都关了，申请的都释放了
                        for(j=0;j<conn_num;j++){
                            if(fd_arr[j]>0)
                                close(fd_arr[j]);
                            if(msg[j].buf!=NULL)
                                free(msg[j].buf);
                        }//end of for
                        return -3;
                    }//end of if
                    for(j=0;j<msg[i].str_len;j++)
                        msg[i].buf[j]=rand()%256;
                    n=write_msg_cli(fd_arr[i],msg[i].buf,msg[i].str_len);
                    if(n<=0){
                        printf("[fd=%d]连接关闭,stat=WRITE_STR \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        //if(n<0)
                            stat[i]=ERR_WRITE;
                        //else
                            //stat[i]=INIT;
                        continue;
                    }
                    tm[i]=0;//计时器清零
                    stat[i]=READ_END;//更新状态
                }//end of STR
                else if(stat[i]==READ_END){//READ_END
                    n=read_msg_cli(fd_arr[i],END,END_READ_LEN);
                    if(n<=0){
                        printf("[fd=%d]连接关闭,stat=READ_END \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_READ;
                        continue;
                    }//end of if
                    stat[i]=WRITE_FILE;//更新状态
                    close(fd_arr[i]);
                    fd_arr[i]=-1;
                    //读写文件
                    int fp;
                    if((fp=myOpen(msg[i].pid))<0){
                        //把打开的都关了，申请的都释放了
                        for(j=0;j<conn_num;j++){
                            if(fd_arr[j]>0)
                                close(fd_arr[j]);
                            if(msg[j].buf!=NULL)
                                free(msg[j].buf);
                        }//end of for
                        return -4;
                    }
                    char tmp[100];
                    sprintf(tmp, "%d\n%d\n%s\n",msg[i].stuno,msg[i].pid,msg[i].time_str);
                    write_msg_cli(fp,tmp,strlen(tmp));
                    write_msg_cli(fp,msg[i].buf,msg[i].str_len);
                    close(fp);
                    stat[i]=COMPLETED;//更新状态
                    //释放申请的空间
                    if(msg[i].buf!=NULL)
                        free(msg[i].buf);
                    completed_fd_num++;
                }//end of else if
                //printf("stat[i]=%d,我也不知道为啥来了\n", stat[i]);
            }//end of if

        }//end of for
    }//end of while
    printf("共发起连接:[%d/%d],已成功:[%d],共失败:[%d]\n",fresh_fd_cnt,conn_num,completed_fd_num,fail_fd_cnt);
    printf("Done!!!\n");

    free(fd_arr);
    free(tm);
    free(msg);
    free(stat);
    free(stat_cnt);
}
