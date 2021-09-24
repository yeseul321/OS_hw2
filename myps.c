#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <termios.h>

#define MAXLEN 1024

char proc_path[MAXLEN];
char checkoption[4];

//파일 디렉토리 명에서 필요 없는 파일을 걸러주는 함수
static int filter(const struct dirent *dirent){
	if(isdigit(dirent->d_name[0])!=0) return 1;
	else return 0;
}

typedef struct Proc{
	char USER[MAXLEN];
	int PID;
	double CPU;
	double MEM;
	long long int VSZ;
	long long int RSS;
	char TTY[MAXLEN];
	char STAT[MAXLEN];
	char START[6];
	char TIME[10];
	char COMMAND[MAXLEN];
}Proc;

//uptime 정보 가져오기
long long int getUptime(void){

	int uptime_fd;
	if((uptime_fd = open("/proc/uptime", O_RDONLY))<0){
		fprintf(stderr, "uptime open error\n");
		exit(1);
	}

	char uptime_tmp[MAXLEN];
	memset(uptime_tmp, 0, sizeof(uptime_tmp));

	read(uptime_fd, uptime_tmp, MAXLEN);
	close(uptime_fd);

	for(int i=0; i<sizeof(uptime_tmp); i++){
		if(uptime_tmp[i]==' '){
			for(int j=i; j<sizeof(uptime_tmp); j++){
				uptime_tmp[j]='\0';
			}
			break;
		}
	}

	long long int uptime=atoll(uptime_tmp);//시스템 부팅후 현재 시각까지의 부팅시간(초)
	//printf("%lld", uptime);

	return uptime;
}

long long getMemTotal(){
	int meminfo_fd;
	long long int memTotal;
	if((meminfo_fd = open("/proc/meminfo", O_RDONLY))<0){
		fprintf(stderr, "uptime open error\n");
		exit(1);
	}
	FILE * meminfo_fp = fdopen(meminfo_fd, "r");
	char tmpbuf[MAXLEN];
	memset(tmpbuf, '\0',sizeof(tmpbuf));
	fgets(tmpbuf, MAXLEN, meminfo_fp);
	tmpbuf[strlen(tmpbuf)]='\0';
	for(int i=0; i<strlen(tmpbuf); i++){
		if(!isdigit(tmpbuf[i])) continue;
		else{
			memTotal = atoll(&tmpbuf[i]);
			break;
		} 
	}
	fclose(meminfo_fp);
	close(meminfo_fd);
	//printf("%lld", memTotal);
	return memTotal;
}

long long getRss(int pid){
	char status_path[MAXLEN];
	memset(status_path, '\0', MAXLEN);
	sprintf(status_path, "/proc/%d/status", pid);
	FILE * status_fp;
	if((status_fp = fopen(status_path, "r"))==NULL){
		fprintf(stderr, "%s fopen error\n", status_path);
		exit(1);
	}
	int check=0;
	char line_tmp[MAXLEN];
	memset(line_tmp,'\0', MAXLEN);
	while(NULL!=fgets(line_tmp, MAXLEN, status_fp)){
		if(strncmp(line_tmp, "VmRSS",5)==0){
			check=1;
			break;
		}
		else {
			memset(line_tmp, '\0', MAXLEN); continue;
		}
	}
	if(check==1){//VmRSS가 존재함
		fclose(status_fp);
		//printf("%s\n", line_tmp);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else return 0;

}

long long getVsz(int pid){
	char status_path[MAXLEN];
	memset(status_path,'\0', MAXLEN);
	sprintf(status_path, "/proc/%d/status", pid);
	FILE * status_fp;
	if((status_fp = fopen(status_path, "r"))==NULL){
		fprintf(stderr, "%s fopen error\n", status_path);
		exit(1);
	}
	int check=0;
	char line_tmp[MAXLEN];
	memset(line_tmp,  '\0', MAXLEN);
	while(NULL!=fgets(line_tmp, MAXLEN, status_fp)){
		if(strncmp(line_tmp, "VmSize",6)==0){
			check=1;
			break;
		}
		else {
			memset(line_tmp,'\0', MAXLEN); continue;
		}
	}
	if(check==1){//VmSize가 존재함
		fclose(status_fp);
		//printf("%s\n", line_tmp);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else return 0;

}

long long int getVmLck(int pid){
	char status_path[MAXLEN];
	memset(status_path,'\0', MAXLEN);
	sprintf(status_path, "/proc/%d/status", pid);
	FILE * status_fp;
	if((status_fp = fopen(status_path, "r"))==NULL){
		fprintf(stderr, "%s fopen error\n", status_path);
		exit(1);
	}
	int check=0;
	char line_tmp[MAXLEN];
	memset(line_tmp,  '\0', MAXLEN);
	while(NULL!=fgets(line_tmp, MAXLEN, status_fp)){
		if(strncmp(line_tmp, "VmLck",5)==0){
			check=1;
			break;
		}
		else {
			memset(line_tmp,'\0', MAXLEN); continue;
		}
	}
	if(check==1){//VmLck가 존재함
		fclose(status_fp);
		//printf("%s\n", line_tmp);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else return 0;

}

int main(int argc, char *argv[]){

	struct winsize size;

	if (ioctl(0, TIOCGWINSZ, (char *)&size) < 0){
		fprintf(stderr, "ioctl error\n");
		exit(1);
	}
	int w_col = size.ws_col;

	fflush(stdout);
	struct dirent **namelist;
	int count=0; //process개수
	memset(proc_path, '\0',sizeof(proc_path));//메모리 초기화

	sprintf(proc_path, "%s", "/proc");//proc디렉토리 경로

	//proc디렉토리 안에 숫자이름인 디렉토리를 가져온다
	if((count = scandir(proc_path, &namelist, *filter, alphasort))==-1){
		fprintf(stderr, "%s Directory Scan Error\n", proc_path);
		exit(1);
	}

	//구조체 배열 생성
	Proc proc_struct[count];

	//구조체 배열 초기화
	memset(proc_struct, 0,sizeof(proc_struct));

	//namelist 안에 저장되어있는 문자열로 저장된 디렉토리 이름을 정수형으로 변환해서 구조체 배열에 저장
	for(int i=0; i<count; i++){
		proc_struct[i].PID=atoi(namelist[i]->d_name);
	}

	//PID 오름차순으로 정렬
	Proc tmp;
	for(int i=0; i<count-1; i++){
		for(int j=i+1; j<count;j++){
			if(proc_struct[i].PID>proc_struct[j].PID){
				tmp=proc_struct[i];
				proc_struct[i]=proc_struct[j];
				proc_struct[j]=tmp;
			} 
			else continue;
		}
	}
	/*
	for(int i=0; i<count; i++){
		printf("%d\n", proc_struct[i].PID);
	}*/

	struct stat statbuf;
	struct passwd *pwd;
	char stat_path[MAXLEN];

	for(int i=0; i<count; i++){

		memset(stat_path, '\0', sizeof(stat_path));
		sprintf(stat_path, "%s/%d", proc_path, proc_struct[i].PID);

		if(stat(stat_path, &statbuf)==-1){//pid로 puid가져오기
			fprintf(stderr, "%d stat error\n", proc_struct[i].PID);
			exit(1);
		}

		if((pwd=getpwuid(statbuf.st_uid))==NULL){//uid로 user 가져오기
			fprintf(stderr, "%d getpwuid error\n", proc_struct[i].PID);		
		}

		char usertmp[MAXLEN];
		memset(usertmp, '\0',MAXLEN);
		strcpy(usertmp, pwd->pw_name);
		if(strlen(usertmp)>8){
			usertmp[7]='+';
			for(int k=8; k<strlen(usertmp); k++){
				usertmp[k]='\0';
			}
		}
		strncpy(proc_struct[i].USER,usertmp,10);

		//stat 내용 불러오기
		int process_fd=0;
		sprintf(stat_path, "%s/stat", stat_path);
		if((process_fd = open(stat_path, O_RDONLY))<0){
			fprintf(stderr, "%s open error\n",stat_path);
			exit(1);
		}

		char process_stat_tmp[1024];
		memset(process_stat_tmp,  0,sizeof(process_stat_tmp));
		read(process_fd, process_stat_tmp, MAXLEN);
		close(process_fd);
		
		int idx=0;	char stoken[MAXLEN][MAXLEN];
		for(int i=0; i<MAXLEN; i++){
			memset(stoken[i], '\0', MAXLEN);
		}

		char *ptr = strtok(process_stat_tmp, " ");
		
		while(ptr != NULL){
			strcpy(stoken[idx++],ptr);
			ptr=strtok(NULL, " ");
		}

		//uptime 정보 가져오기
		long long int uptime=getUptime();//시스템 부팅후 현재 시각까지의 부팅시간(초)
		//%CPU 계산
		long long int totalTime=atoll(stoken[13])+atoll(stoken[14]);
		double totalTime_tic = (double)totalTime/sysconf(_SC_CLK_TCK);
		proc_struct[i].CPU = (double)totalTime_tic/uptime*100;

		//memTotal 정보 가져오기
		long long int memTotal = getMemTotal();
		long long int rss=getRss(proc_struct[i].PID);
		//%MEM 계산
		proc_struct[i].MEM=(double)rss/memTotal*100;

		//VSZ가져오기
		proc_struct[i].VSZ=getVsz(proc_struct[i].PID); 
		proc_struct[i].RSS=rss;

		//COMMAND가져오기
		char cmdline_path[MAXLEN];
		memset(cmdline_path, '\0', MAXLEN);
		sprintf(cmdline_path, "/proc/%d/cmdline", proc_struct[i].PID);
		FILE * cmdline_fp = fopen(cmdline_path, "r");

		char command[MAXLEN];
		memset(command,  '\0',MAXLEN);
		
		if(NULL!=fgets(command, MAXLEN, cmdline_fp)){
			for(int i=0; i<sizeof(command); i++){
				if((command[i]=='\0')&&(command[i+1]!='\0')){
					command[i]=' ';
				}
			}
			command[strlen(command)]='\0';
			memset(proc_struct[i].COMMAND, '\0', MAXLEN);
			strcpy(proc_struct[i].COMMAND, command);
			fclose(cmdline_fp);
		}
		else{//comm파일을 열어야함
			char comm_path[MAXLEN];
			memset(comm_path,'\0', MAXLEN);
			sprintf(comm_path, "/proc/%d/comm", proc_struct[i].PID);
			FILE * comm_fp = fopen(comm_path, "r");
			if(NULL==fgets(command, MAXLEN, comm_fp)){
				fprintf(stderr, "엥 경로가 없는딩\n");
				exit(1);
			}
			else{
				for(int i=0; i<strlen(command); i++){
					if(command[i]=='\n'){
						command[i]='\0';
					}
				}

				memset(proc_struct[i].COMMAND,  '\0' , MAXLEN);
				sprintf(proc_struct[i].COMMAND, "[%s]", command);

				fclose(comm_fp);
			}
			memset(command, '\0', MAXLEN);
		}

   
		//TIME 구하기
		int secTime = (int)totalTime_tic;
		int minTime= secTime/60;
		memset(proc_struct[i].TIME,  '\0', sizeof(proc_struct[i].TIME));
		sprintf(proc_struct[i].TIME, "%d:%02d", minTime, secTime-minTime*60);
		
		//START 구하기
		time_t fulltime;
		struct tm *lt;

		fulltime = time(NULL);
		//printf("%ld\n", fulltime);
		time_t start_time =  fulltime-uptime+(atoi(stoken[21])/sysconf(_SC_CLK_TCK));
		memset(proc_struct[i].START, '\0', sizeof(proc_struct[i].START));
		lt = localtime(&start_time);
		sprintf(proc_struct[i].START,"%02d:%02d", lt->tm_hour, lt->tm_min);

		//TTY 구하기
		memset(proc_struct[i].TTY, '\0', MAXLEN);
		int tty_nr = atoi(stoken[6]);
		if(tty_nr==0){
			strcpy(proc_struct[i].TTY, "?"); 
		}
		else{
			int maj = major(tty_nr);
			int min = minor(tty_nr);
			char link_tmp[MAXLEN];
			memset(link_tmp,'\0', MAXLEN);
			sprintf(link_tmp, "/dev/char/%d:   -%d", maj, min);
			//printf("%d %s\n",proc_struct[i].PID, link_tmp);

			char link_info[MAXLEN];
			memset(link_info, '\0', MAXLEN);

			if(access(link_tmp,F_OK)!=0) {
				sprintf(proc_struct[i].TTY, "pts/%d", min);
			}
			else{
				readlink(link_tmp, link_info, MAXLEN);
				strcpy(proc_struct[i].TTY, link_info+3);
			}

			//printf("%d %d\n", maj, min);
		}

		//STAT구하기
		memset(proc_struct[i].STAT, '\0', MAXLEN);
		char stat_tmp[MAXLEN];
		memset(stat_tmp, '\0', MAXLEN);

		strcat(stat_tmp, stoken[2]);

		if(proc_struct[i].PID==atoi(stoken[5])){
			strcat(stat_tmp, "s");
		}

		if(atoi(stoken[18])>0){
			strcat(stat_tmp, "N");
		}
		else if(atoi(stoken[18])<0){
			strcat(stat_tmp, "<");
		}

		if(getVmLck(proc_struct[i].PID)!=0){
			strcat(stat_tmp, "L");
		}
   
		if(atoi(stoken[19])>1){
			strcat(stat_tmp, "l");
		}

		if(getpgid(proc_struct[i].PID)==atoi(stoken[7])){
			strcat(stat_tmp, "+");
		}

		strcpy(proc_struct[i].STAT, stat_tmp);

	}

	if(argc<1||argc>2){
		fprintf(stderr,"usage : pps [option]\n");
		exit(1);
	}
	else{
		memset(checkoption, 0, sizeof(checkoption));
		if(argc==2){//옵션이 입력됨
			for(int i=0; i<strlen(argv[1]); i++){
				if(argv[1][i]=='u'){
					checkoption[0]=1;
				}
				else if(argv[1][i]=='a'){
					checkoption[1]=1;
				}
				else if(argv[1][i]=='x'){
					checkoption[2]=1;
				}
			}
		}
	}

	//출력 형식 배열에 저장하기
	char print[MAXLEN][MAXLEN];
	for(int i=0; i<MAXLEN; i++){
		memset(print[i], 0, MAXLEN);
	}

		
	///print pps 
	//u : checkoption[0]
	//a : checkoption[1]
	//x : checkoption[2]
	
	int p=1;
	if(checkoption[0]==1){
		snprintf(print[0],w_col,"%-10s%5s%5s%5s%8s%7s %-9s%-5s%7s%7s %-8s", "USER", "PID", "%CPU", "%MEM" ,"VSZ", "RSS","TTY","STAT", "START", "TIME", "COMMAND");
		if(checkoption[1]==1){ 
			if(checkoption[2]==1){//aux 모든것을 출력
				for(int i=0; i<count; i++){
					snprintf(print[p++],w_col,"%-10s%5d%5.1lf%5.1lf%8lld%7lld %-9s%-5s%7s%7s %-8s", 
						proc_struct[i].USER, 
						proc_struct[i].PID, 
						proc_struct[i].CPU, 
						proc_struct[i].MEM,
						proc_struct[i].VSZ,
						proc_struct[i].RSS,
						proc_struct[i].TTY,
						proc_struct[i].STAT,
						proc_struct[i].START,
						proc_struct[i].TIME,
						proc_struct[i].COMMAND);
				}
			}
			else{//au :형식모두 출력인데 tty가 ? 가 아닌거
				for(int i=0; i<count; i++){
					if(strcmp(proc_struct[i].TTY, "?")!=0){
						snprintf(print[p++],w_col,"%-10s%5d%5.1lf%5.1lf%8lld%7lld %-9s%-5s%7s%7s %-8s", 
							proc_struct[i].USER, 
							proc_struct[i].PID, 
							proc_struct[i].CPU, 
							proc_struct[i].MEM,
							proc_struct[i].VSZ,
							proc_struct[i].RSS,
							proc_struct[i].TTY,
							proc_struct[i].STAT,
							proc_struct[i].START,
							proc_struct[i].TIME,
							proc_struct[i].COMMAND);
					}
				}
			}
		}
		else{
			if(checkoption[2]==1){//ux : 형식 모두 출력인데 user 인거
 				for(int i=0; i<count; i++){
					if(strcmp(proc_struct[i].USER, pwd->pw_name)==0){
						snprintf(print[p++],w_col,"%-10s%5d%5.1lf%5.1lf%8lld%7lld %-9s%-5s%7s%7s %-8s", 
							proc_struct[i].USER, 
							proc_struct[i].PID, 
							proc_struct[i].CPU, 
							proc_struct[i].MEM,
							proc_struct[i].VSZ,
							proc_struct[i].RSS,
							proc_struct[i].TTY,
							proc_struct[i].STAT,
							proc_struct[i].START,
							proc_struct[i].TIME,
							proc_struct[i].COMMAND);
					}
				}
			}
			else{//u 
				for(int i=0; i<count; i++){
					if(strcmp(proc_struct[i].USER, pwd->pw_name)==0){
						if(strcmp(proc_struct[i].TTY, "?")!=0){
							snprintf(print[p++],w_col,"%-10s%5d%5.1lf%5.1lf%8lld%7lld %-9s%-5s%7s%7s %-8s", 
								proc_struct[i].USER, 
								proc_struct[i].PID, 
								proc_struct[i].CPU, 
								proc_struct[i].MEM,
								proc_struct[i].VSZ,
								proc_struct[i].RSS,
								proc_struct[i].TTY,
								proc_struct[i].STAT,
								proc_struct[i].START,
								proc_struct[i].TIME,
								proc_struct[i].COMMAND);
						}
					}
				}
			}
		}
	}
	else{
		snprintf(print[0],w_col,"%5s %-9s%-5s%7s %-8s","PID", "TTY","STAT", "TIME", "COMMAND");
		if(checkoption[1]==1){
			if(checkoption[2]==1){//ax
				for(int i=0; i<count; i++){
						snprintf(print[p++],w_col,"%5d %-9s%-5s%7s %-8s",  
							proc_struct[i].PID, 
							proc_struct[i].TTY,
							proc_struct[i].STAT,
							proc_struct[i].TIME,
							proc_struct[i].COMMAND);
				}
			}
			else{//a
					for(int i=0; i<count; i++){
						if(strcmp(proc_struct[i].TTY, "?")!=0){
							snprintf(print[p++],w_col,"%5d %-9s%-5s%7s %-8s",  
								proc_struct[i].PID, 
								proc_struct[i].TTY,
								proc_struct[i].STAT,
								proc_struct[i].TIME,
								proc_struct[i].COMMAND);
						}
					}
			}
		}
		else{
			if(checkoption[2]==1){//x
				for(int i=0; i<count; i++){
					if(strcmp(proc_struct[i].USER, pwd->pw_name)==0){
						snprintf(print[p++],w_col,"%5d %-9s%-5s%7s %-8s",  
							proc_struct[i].PID, 
							proc_struct[i].TTY,
							proc_struct[i].STAT,
							proc_struct[i].TIME,
							proc_struct[i].COMMAND);
					}
				}
			}
			else{//옵션 없을 때
				memset(print[0], 0, sizeof(print[0]));
				snprintf(print[0],w_col,"%5s %-9s%9s %-8s","PID", "TTY","TIME", "COMMAND");
				char *ret, tty[MAXLEN];
				memset(tty, '\0', MAXLEN);
				if((ret = ttyname(STDERR_FILENO))==NULL){
					fprintf(stderr, "ttyname() error\n");
					exit(1);
				}
				strcpy(tty, ret+5);
				for(int i=0; i<count; i++){
					if(strcmp(tty, proc_struct[i].TTY)==0){
						char timebuf[MAXLEN];
						memset(timebuf, 0, MAXLEN);
						strcpy(timebuf, proc_struct[i].TIME);
						if(atoi(timebuf)>=60){
							int h = atoi(timebuf)/60;
							int m = atoi(timebuf)-h*60;
							for(int k=0; k<strlen(timebuf); k++){
								if(timebuf[k]==':'){
									sprintf(proc_struct[i].TIME, "%02d:%02d%s", h, m, &timebuf[k]);
									break;
								}
							}
						}
						else{
							if(atoi(timebuf)<10){
								sprintf(proc_struct[i].TIME, "00:0%s", timebuf);
							}
							else{
								sprintf(proc_struct[i].TIME, "00:%s", timebuf);
							}
						}
						sprintf(print[p++],"%5d %-9s%9s %-8s",  
							proc_struct[i].PID, 
							proc_struct[i].TTY,
							proc_struct[i].TIME,
							proc_struct[i].COMMAND);
					}
				}
			}
		}
	}
	for(int i=0; i<p; i++){	
		printf("%s\n", print[i]);	
	}
/*
   for(int i=0; i<count; i++){
   printf("%s\n", proc_struct[i].USER);
   }*/
}
