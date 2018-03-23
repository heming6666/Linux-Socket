#include "cli.h"
int time_flag=0;//�Ѵ�����ʱ������
void alarm_handler(){
    time_flag++;
    alarm(TIMER_PIECE);
}
//nofork��ʽ
int nonFork(char *remote_ip,int remote_port,int conn_num){
    int *fd_arr;//���ڴ��ÿ�����뵽��fd
    fd_arr=(int*)malloc(sizeof(int)*conn_num);
    int fd_cnt=0;//ÿ�δ�����fd��Ŀ

    int *tm;//���ڴ��ÿ��fd�ȴ�ʱ��
    tm=(int *)malloc(sizeof(int)*conn_num);

    int *stat;//��ʾÿ�����ӵ�״̬
    stat=(int *)malloc(sizeof(int)*conn_num);

    msg_cli *msg;//���ڴ���շ�����
    msg=(msg_cli *)malloc(sizeof(msg_cli)*conn_num);

    int network_order_msg_temp;//��������ת��Ϊ������ʱ��ʱ����
    int fresh_fd_cnt=0;//��¼�Ѿ����벢�����˶��ٸ�fd
    int fail_fd_cnt=0;//��¼�ܼƶ��ٸ�fd����ʧ�ܵ�
    int completed_fd_num=0;//������շ����ݵ�sockfd���
    int *stat_cnt;
    stat_cnt=(int *)malloc(sizeof(int)*STAT_CNT);
    char log_time[20];
    int i,j,n;
    fd_set rfd;
    int maxfd;
    int sel;
    pid_t pid=getpid();

    int rfd_cnt=0;

    //��ʼ������
    for(i=0;i<conn_num;i++){
        fd_arr[i]=-1;
        tm[i]=0;
        stat[i]=INIT;
        msg[i].stuno=1552259;
        msg[i].pid=(pid<<16)+i;
    }

    // ��ʱ��
    signal(SIGALRM, alarm_handler);
    alarm(TIMER_PIECE);
    while (1) {
        //����ȫ�����
        if(completed_fd_num>=conn_num)
            break;
        //ͳ��״̬��������0��ʼ��
        for(i=0;i<STAT_CNT;i++){
            stat_cnt[i]=0;
        }
        //��ʱ��������
        if(time_flag>0){
            for(i=0;i<conn_num;i++){
                if((stat[i]!=INIT)&&(stat[i]!=COMPLETED)){//�����û����fd/�Ѿ����µ����ü�ʱ
                    tm[i]+=time_flag;//�ۼ�
                    //����Ѿ���ʱ�������������
                    //1����ʱû��Ӧ��������������
                    //2������ĳЩԭ�������жϷ���-1���Ѿ������㹻��ʱ�䣬��������������
                    if(tm[i]>=RETRY_TIME){
                        if(stat[i]<COMPLETED){//���1,��Ҫ�ر�����
                            close(fd_arr[i]);
                            fd_arr[i]=-1;
                            fail_fd_cnt++;
                        }//end of if
                        stat[i]=INIT;//����Ϊ�����������ӵ�״̬
                        tm[i]=0;//��ʱ������
                    }//end of if
                }//end of if
                if(stat[i]<COMPLETED){
                    stat_cnt[stat[i]]++;//ͳ�ƴ��ڸ���״̬fd����
                }
            }//end of for
            printf("����������:[%d/%d],�ѳɹ�:[%d],��ʧ��:[%d]\n",fresh_fd_cnt,conn_num,completed_fd_num,fail_fd_cnt);
            //��ӡͳ����Ϣ
            for(i=0;i<STAT_CNT;i++){
                if(stat_cnt[i]>0)
                    printf("[stat=%d] : %d�� \n",i, stat_cnt[i]);
            }
            printf("--------\n");
        }//end of if
        time_flag=0;

        //����socket��һ��ֻ����ָ����Ŀ��
        fd_cnt=0;
        for(i=0;i<conn_num;i++){
            if(stat[i]==INIT){//���ڳ�ʼ״̬����������socket
                //����socket
                if((fd_arr[i]=get_socket_cli(remote_ip,remote_port,NON_BLOCK))< 0){
                    return -1;
                };
                fresh_fd_cnt++;
                stat[i]=READ_STUNO;//�޸�״̬
                fd_cnt++;//�����Ѿ������˶��ٸ�
                if(fd_cnt>FD_CNT_PER_TIME)
                    break;
            }//end of if
        }//end of for

        FD_ZERO(&rfd);
        maxfd=0;
        for(i=0;i<conn_num;i++){
            //�ȴ��շ�����״̬
            if((stat[i]>=READ_STUNO)&&(stat[i]<=READ_END)){
                FD_SET(fd_arr[i],&rfd);//�ö���
            }
            //����maxfd
            if(fd_arr[i]>maxfd)
                maxfd=fd_arr[i];
        }//end of for
        sel=select(maxfd + 1, &rfd, NULL, NULL,NULL);
        //select����<=0���粻���ж�����������أ�����continue����whileѭ��
        if(sel<=0){
            if (errno!=EINTR){
                printf("select error: %s(errno: %d)\n", strerror(errno),errno);
                //�Ѵ򿪵Ķ����ˣ�����Ķ��ͷ���
                for(i=0;i<conn_num;i++){
                    if(fd_arr[i]>0)
                        close(fd_arr[i]);
                    if(msg[i].buf!=NULL)
                        free(msg[i].buf);
                }//end of for
                return -2;//���󷵻�
            }//end of if
            else
                continue;
        }//end of if

        //��һ����д��
        for(i=0;i<conn_num;i++){
            //��û����socket��ֱ������
            if(fd_arr[i]<0)
                continue;
            //rfd��Ч�������շ�����
            if (FD_ISSET(fd_arr[i], &rfd)){
                FD_CLR(fd_arr[i], &rfd);
                //printf("fd=%d,��Ҳ��֪��Ϊɶ����\n", fd_arr[i]);
                //printf("stat[i]=%d,��Ҳ��֪��Ϊɶ����\n", stat[i]);
                if(stat[i]==READ_STUNO){//READ_STUNO
                    n=read_msg_cli(fd_arr[i],STUNO,STUNO_READ_LEN);
                    if(n<=0){
                        printf("[fd=%d]���ӹر�,stat=READ_STUNO \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        //�������-1��read������ERR_READ,��ȴ�һ��ʱ��������
                        //�������0���Է��ر����ӣ�����ֱ����������
                        stat[i]=ERR_READ;
                        continue;
                    }//end of if
                    stat[i]=WRITE_STUNO;//����״̬
                    //�Ȱ�������ת��Ϊ������
                    network_order_msg_temp=htonl(msg[i].stuno);
                    n=write_msg_cli(fd_arr[i],(char *)&network_order_msg_temp,STUNO_WRITE_LEN);
                    if(n<=0){
                        printf("[fd=%d]���ӹر�,stat=WRITE_STUNO \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_WRITE;
                        continue;
                    }
                    tm[i]=0;//��ʱ������
                    stat[i]=READ_PID;//����״̬
                }//end of StuNo
                else if(stat[i]==READ_PID){//READ_PID
                    n=read_msg_cli(fd_arr[i],PID,PID_READ_LEN);
                    if(n<=0){
                        printf("[fd=%d]���ӹر�,stat=READ_PID \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_READ;
                        continue;
                    }//end of if
                    stat[i]=WRITE_STUNO;//����״̬
                    //�Ȱ�������ת��Ϊ������
                    network_order_msg_temp=htonl(msg[i].pid);
                    n=write_msg_cli(fd_arr[i],(char *)&network_order_msg_temp,PID_WRITE_LEN);
                    if(n<=0){
                        printf("[fd=%d]���ӹر�,stat=WRITE_PID \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_WRITE;

                        continue;
                    }
                    tm[i]=0;//��ʱ������
                    stat[i]=READ_TIME;//����״̬
                }//end of PID
                else if(stat[i]==READ_TIME){//READ_TIME
                    n=read_msg_cli(fd_arr[i],TIME,TIME_READ_LEN);
                    if(n<=0){
                        printf("[fd=%d]���ӹر�,stat=READ_TIME \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_READ;
                        continue;
                    }//end of if
                    stat[i]=WRITE_TIME;//����״̬
                    getTime(msg[i].time_str);
                    n=write_msg_cli(fd_arr[i],msg[i].time_str,TIME_WRITE_LEN);
                    if(n<=0){
                        printf("[fd=%d]���ӹر�,stat=WRITE_TIME \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_WRITE;
                        continue;
                    }
                    tm[i]=0;//��ʱ������
                    stat[i]=READ_STR;//����״̬
                }//end of TIME
                else if(stat[i]==READ_STR){//READ_STR
                    msg[i].str_len=read_msg_cli(fd_arr[i],STR,STR_READ_LEN);
                    if(n<=0){
                        printf("[fd=%d]���ӹر�,stat=READ_STR \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        //if(n<0)
                            stat[i]=ERR_READ;
                        //else
                            //stat[i]=INIT
                        continue;
                    }//end of if
                    stat[i]=WRITE_STR;//����״̬
                    msg[i].buf=(char *)malloc(msg[i].str_len);
                    if(msg[i].buf==NULL){
                        //�Ѵ򿪵Ķ����ˣ�����Ķ��ͷ���
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
                        printf("[fd=%d]���ӹر�,stat=WRITE_STR \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        //if(n<0)
                            stat[i]=ERR_WRITE;
                        //else
                            //stat[i]=INIT;
                        continue;
                    }
                    tm[i]=0;//��ʱ������
                    stat[i]=READ_END;//����״̬
                }//end of STR
                else if(stat[i]==READ_END){//READ_END
                    n=read_msg_cli(fd_arr[i],END,END_READ_LEN);
                    if(n<=0){
                        printf("[fd=%d]���ӹر�,stat=READ_END \n",fd_arr[i]);
                        close(fd_arr[i]);
                        fd_arr[i]=-1;
                        fail_fd_cnt++;
                        stat[i]=ERR_READ;
                        continue;
                    }//end of if
                    stat[i]=WRITE_FILE;//����״̬
                    close(fd_arr[i]);
                    fd_arr[i]=-1;
                    //��д�ļ�
                    int fp;
                    if((fp=myOpen(msg[i].pid))<0){
                        //�Ѵ򿪵Ķ����ˣ�����Ķ��ͷ���
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
                    stat[i]=COMPLETED;//����״̬
                    //�ͷ�����Ŀռ�
                    if(msg[i].buf!=NULL)
                        free(msg[i].buf);
                    completed_fd_num++;
                }//end of else if
                //printf("stat[i]=%d,��Ҳ��֪��Ϊɶ����\n", stat[i]);
            }//end of if

        }//end of for
    }//end of while
    printf("����������:[%d/%d],�ѳɹ�:[%d],��ʧ��:[%d]\n",fresh_fd_cnt,conn_num,completed_fd_num,fail_fd_cnt);
    printf("Done!!!\n");

    free(fd_arr);
    free(tm);
    free(msg);
    free(stat);
    free(stat_cnt);
}
