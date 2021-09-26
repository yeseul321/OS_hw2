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
#include <signal.h>
#include <utmp.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <ncurses.h>

#define MAXLEN 1024

int start_line;
int w_row, w_col;
typedef struct Proc{
	int PID;
	char USER[MAXLEN]; //user 이름
	char PR[3]; //우선순위
	int NI; //nice 값
	long long int VIRT; //가상 메모리 사용량
	long long RES; //실제 메모리 사용량
	long long SHR; //공유 메모리 사용량
	char S; //stat
	double CPU; //CPU 사용률
	double MEM; //메모리 사용률
	char TIME[8]; //총 cpu 사용시간
	char COMMAND[MAXLEN]; //option이 있을 경우에 출력되는 command
}Proc;

//구조체 배열 생성
Proc proc_struct[MAXLEN];
char proc_path[MAXLEN];
char head[5][MAXLEN];
int proc_count, run, slep, stop, zom;
long long int uptime;

int ccpu[9];
int bcpu[9];

void sigint_handler(int signo);
void ttop_print();
void save_data();
int ggetch();

int ggetch(void){ 
   int ch;
   struct termios oldt, newt;
   tcgetattr(0,&oldt);
   newt = oldt;
   newt.c_lflag &= ~ICANON;
   newt.c_lflag &= ~ECHO;
   //newt.c_lflag &= ~(ICANON|ECHO);
   tcsetattr(0, TCSAFLUSH, &newt);
   ch = getchar();
   tcsetattr(0, TCSAFLUSH, &oldt);
   return ch;
}

//파일 디렉토리 명에서 필요 없는 파일을 걸러주는 함수
static int s_filter(const struct dirent *dirent){
	if(isdigit(dirent->d_name[0])!=0) return 1;
	else return 0;
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

	return memTotal;
}

long long getMemFree(){
	int meminfo_fd;   
	long long int memFree;
	if((meminfo_fd = open("/proc/meminfo", O_RDONLY))<0){
		fprintf(stderr, "meminfo open error\n");
		exit(1);
	}
	FILE * meminfo_fp = fdopen(meminfo_fd, "r");
	int check=0;
	char line_tmp[MAXLEN];
	memset(line_tmp,'\0', MAXLEN);
	while(NULL!=fgets(line_tmp, MAXLEN, meminfo_fp)){
		if(strncmp(line_tmp, "MemFree",7)==0){
			check=1;
			break;
		}
		else {
			memset(line_tmp, '\0', MAXLEN); continue;
		}
	}
	if(check==1){
		close(meminfo_fd);
		fclose(meminfo_fp);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else {fclose(meminfo_fp); close(meminfo_fd); return 0;}
}

long long getBuffer(){
	int meminfo_fd;   
	long long int memFree;
	if((meminfo_fd = open("/proc/meminfo", O_RDONLY))<0){
		fprintf(stderr, "meminfo open error\n");
		exit(1);
	}
	FILE * meminfo_fp = fdopen(meminfo_fd, "r");
	int check=0;
	char line_tmp[MAXLEN];
	memset(line_tmp,'\0', MAXLEN);
	while(NULL!=fgets(line_tmp, MAXLEN, meminfo_fp)){
		if(strncmp(line_tmp, "Buffers",7)==0){
			check=1;
			break;
		}
		else {
			memset(line_tmp, '\0', MAXLEN); continue;
		}
	}
	if(check==1){
		fclose(meminfo_fp);
		close(meminfo_fd);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else {fclose(meminfo_fp); close(meminfo_fd); return 0;}
}

long long getCache(){
	int meminfo_fd;   
	long long int memFree;
	if((meminfo_fd = open("/proc/meminfo", O_RDONLY))<0){
		fprintf(stderr, "meminfo open error\n");
		exit(1);
	}
	FILE * meminfo_fp = fdopen(meminfo_fd, "r");
	int check=0;
	char line_tmp[MAXLEN];
	memset(line_tmp,'\0', MAXLEN);
	while(NULL!=fgets(line_tmp, MAXLEN, meminfo_fp)){
		if(strncmp(line_tmp, "Cached",6)==0){
			check=1;
			break;
		}
		else {
			memset(line_tmp, '\0', MAXLEN); continue;
		}
	}
	if(check==1){
		fclose(meminfo_fp);
		close(meminfo_fd);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else {fclose(meminfo_fp); close(meminfo_fd); return 0;}
}

long long getSR(){
	int meminfo_fd;   
	long long int memFree;
	if((meminfo_fd = open("/proc/meminfo", O_RDONLY))<0){
		fprintf(stderr, "meminfo open error\n");
		exit(1);
	}
	FILE * meminfo_fp = fdopen(meminfo_fd, "r");
	int check=0;
	char line_tmp[MAXLEN];
	memset(line_tmp,'\0', MAXLEN);
	while(NULL!=fgets(line_tmp, MAXLEN, meminfo_fp)){
		if(strncmp(line_tmp, "SReclaimable",12)==0){
			check=1;
			break;
		}
		else {
			memset(line_tmp, '\0', MAXLEN); continue;
		}
	}
	if(check==1){
		fclose(meminfo_fp);
		close(meminfo_fd);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else {fclose(meminfo_fp); close(meminfo_fd); return 0;}
}

long long getSwapTotal(){
	int meminfo_fd;   
	long long int memFree;
	if((meminfo_fd = open("/proc/meminfo", O_RDONLY))<0){
		fprintf(stderr, "meminfo open error\n");
		exit(1);
	}
	FILE * meminfo_fp = fdopen(meminfo_fd, "r");
	int check=0;
	char line_tmp[MAXLEN];
	memset(line_tmp,'\0', MAXLEN);
	while(NULL!=fgets(line_tmp, MAXLEN, meminfo_fp)){
		if(strncmp(line_tmp, "SwapTotal",8)==0){
			check=1;
			break;
		}
		else {
			memset(line_tmp, '\0', MAXLEN); continue;
		}
	}
	if(check==1){
		fclose(meminfo_fp);
		close(meminfo_fd);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else return 0;
}

long long getSwapFree(){
	int meminfo_fd;   
	long long int memFree;
	if((meminfo_fd = open("/proc/meminfo", O_RDONLY))<0){
		fprintf(stderr, "meminfo open error\n");
		exit(1);
	}
	FILE * meminfo_fp = fdopen(meminfo_fd, "r");
	int check=0;
	char line_tmp[MAXLEN];
	memset(line_tmp,'\0', MAXLEN);
	while(NULL!=fgets(line_tmp, MAXLEN, meminfo_fp)){
		if(strncmp(line_tmp, "SwapFree",8)==0){
			check=1;
			break;
		}
		else {
			memset(line_tmp, '\0', MAXLEN); continue;
		}
	}
	if(check==1){
		fclose(meminfo_fp);
		close(meminfo_fd);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else return 0;
}

long long getMemAvail(){
	int meminfo_fd;     
	long long int memFree;
	if((meminfo_fd = open("/proc/meminfo", O_RDONLY))<0){
		fprintf(stderr, "meminfo open error\n");
		exit(1);
	}
	FILE * meminfo_fp = fdopen(meminfo_fd, "r");
	int check=0;
	char line_tmp[MAXLEN];
	memset(line_tmp,'\0', MAXLEN);
	while(NULL!=fgets(line_tmp, MAXLEN, meminfo_fp)){
		if(strncmp(line_tmp, "MemAvailable",12)==0){
			check=1;
			break;
		}
		else {
			memset(line_tmp, '\0', MAXLEN); continue;
		}
	}
	if(check==1){
		fclose(meminfo_fp);
		close(meminfo_fd);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else return 0;
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
	else {fclose(status_fp);return 0;}
}
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

long long getVmSize(int pid){
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
		//printf("VM%s\n", line_tmp);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else {fclose(status_fp);return 0;}

}

long long getRssShmem(int pid){
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
		if(strncmp(line_tmp, "RssFile",7)==0){
			check=1;
			break;
		}
		else {
			memset(line_tmp,'\0', MAXLEN); continue;
		}
	}
	if(check==1){//RssFile이 존재함
		fclose(status_fp);
		for(int i=0; i<strlen(line_tmp); i++){
			if(isdigit(line_tmp[i])) return atoll(&line_tmp[i]);
		}
	}
	else {
		fclose(status_fp);
		return 0;
	}

}

int getUser(){
	int user=0;
	struct utmp *uutmp;
	setutent();
	while((uutmp = getutent())!=NULL){
		if(uutmp->ut_type == USER_PROCESS) user++;
	}
	endutent();
	return user;
}

void sigint_handler(int signo){
	write(1,"\33[3J\33[H\33[2J", 11);
	
	run=0; slep=0; stop=0; zom=0;
	save_data();
	ttop_print();
	alarm(3);
}

void makehead(){

	time_t t;
	struct tm *lt;

	t = time(NULL);
	lt = localtime(&t);
	uptime=getUptime();

	int up_h = uptime/3600;
	int up_m = (uptime - up_h*3600)/60;

	FILE * loadavg_fp;

	if((loadavg_fp = fopen("/proc/loadavg", "r"))==NULL){
		fprintf(stderr, "loadavg fopen error\n");
		exit(1);
	}

	double l1=0,l2=0,l3=0;
	fscanf(loadavg_fp,"%lf %lf %lf", &l1, &l2, &l3);
	fclose(loadavg_fp);

	FILE * stat_fp;

	if((stat_fp = fopen("/proc/stat", "r"))==NULL){
		fprintf(stderr, "/proc/stat fopen error\n");
		exit(1);
	}

	long long int sum=0;
	char tmp[MAXLEN];
	memset(tmp, 0, MAXLEN);
	
	fscanf(stat_fp, "%s", tmp);

	for(int i=0; i<8; i++){
		fscanf(stat_fp, "%d", &ccpu[i]);
		//printf("%d ", ccpu[i]);
		ccpu[i] = ccpu[i]/4;
		sum += ccpu[i];
	}
	ccpu[8]=sum;

	double us=(double)(ccpu[0]-bcpu[0])/(double)(ccpu[8]-bcpu[8])*100;
	double sy=(double)(ccpu[2]-bcpu[2])/(double)(ccpu[8]-bcpu[8])*100;
	double ui=(double)(ccpu[1]-bcpu[1])/(double)(ccpu[8]-bcpu[8])*100;
	double id=(double)(ccpu[3]-bcpu[3])/(double)(ccpu[8]-bcpu[8])*100;
	double wa=(double)(ccpu[4]-bcpu[4])/(double)(ccpu[8]-bcpu[8])*100;
	double hi=(double)(ccpu[5]-bcpu[5])/(double)(ccpu[8]-bcpu[8])*100;
	double si=(double)(ccpu[6]-bcpu[6])/(double)(ccpu[8]-bcpu[8])*100;
	double st=(double)(ccpu[7]-bcpu[7])/(double)(ccpu[8]-bcpu[8])*100;	
	
	memcpy(bcpu, ccpu, sizeof(bcpu));
	fclose(stat_fp);

	double memTotal = (double)getMemTotal()/1024;
	double memFree = (double)getMemFree()/1024;		//%CPU 계산

	double memUsed = memTotal-memFree-((double)getBuffer()/1024)-((double)getCache()/1024)-((double)getSR()/1024);
	double membc = ((double)getBuffer()/1024)+((double)getCache()/1024)+((double)getSR()/1024);

	double swapTotal = (double)getSwapTotal()/1024;
	double swapFree = (double)getSwapFree()/1024;
	double swapUsed = swapTotal-swapFree;		//%CPU 계산

	double MemAvail = (double)getMemAvail()/1024;

	int user=getUser();
	memset(head, 0, sizeof(head));

	sprintf(head[0],"top - %02d:%02d:%02d up  %02d:%02d, %d user,  load average: %3.2lf, %3.2lf, %3.2lf", lt->tm_hour, lt->tm_min, lt->tm_sec, up_h, up_m, user, l1, l2, l3);
	sprintf(head[1],"Tasks: %3d total, %3d running, %3d sleeping %3d stopped, %3d zombie", proc_count, run, slep, stop, zom);
	sprintf(head[2],"%%Cpu(s): %3.1lf us, %3.1lf sy, %3.1lf ni, %3.1lf id, %3.1lf wa, %3.1lf hi, %3.1lf si, %3.1lf st", us,sy,ui,id,wa,hi,si,st);
	sprintf(head[3],"MiB Mem : %.1f total, %.1f free, %.1f used, %.1f buff/cache", memTotal, memFree, memUsed, membc);
	sprintf(head[4],"MiB Swap: %.1f total, %.1f free, %.1f used, %.1f avail Mem", swapTotal, swapFree, swapUsed, MemAvail);

}

void ttop_print(){
	//터미널의 가로 세로 크기 구하기
	struct winsize size;

	if (ioctl(0, TIOCGWINSZ, (char *)&size) < 0){
		fprintf(stderr, "ioctl error\n");
		exit(1);
	}

	w_row = size.ws_row;
	w_col = size.ws_col;

	char print[MAXLEN][MAXLEN];
	for(int i=0; i<MAXLEN; i++){
		memset(print, '\0', MAXLEN);
	}

	makehead();
	//위에 내용
	for(int i=0; i<5; i++){
		strncpy(print[i], head[i], w_col);
	}

	memset(print[5], ' ' , w_col);
	//첫번째 고정 메뉴얼
	snprintf(print[6], w_col,"%5s %-8s %3s %3s %7s %6s %6s %c %4s %4s   %7s %s",
			"PID", "USER", "PR", "NI", "VIRT", "RES", "SHR", 'S', "%CPU", "%MEM", "TIME+", "COMMAND");
	
	int fnum=7;
	for(int i=0; i<proc_count;i++){
		if(strncmp(proc_struct[i].USER, "yeseul", 6)==0){
			
		}
	}
	for(int i=fnum; i<w_row-1+start_line; i++){
		snprintf(print[i], w_col, "%5d %-8s %3s %3d %7lld %6lld %6lld %c %4.1lf %4.1lf   %7s %s",
				proc_struct[i-fnum+start_line].PID, proc_struct[i-fnum+start_line].USER, proc_struct[i-fnum+start_line].PR, proc_struct[i-fnum+start_line].NI,
				proc_struct[i-fnum+start_line].VIRT, proc_struct[i-fnum+start_line].RES, proc_struct[i-fnum+start_line].SHR, proc_struct[i-fnum+start_line].S, 
				proc_struct[i-fnum+start_line].CPU, proc_struct[i-fnum+start_line].MEM, proc_struct[i-fnum+start_line].TIME, proc_struct[i-fnum+start_line].COMMAND);
	}
	for(int i=0; i<w_row-1; i++){	
		printf("%s\n", print[i]);
	} 

}

void save_data(){
	struct dirent **namelist;
	int count=0; //process개수
	memset(proc_path, '\0',sizeof(proc_path));//메모리 초기화

	sprintf(proc_path, "%s", "/proc");//proc디렉토리 경로

	//proc디렉토리 안에 숫자이름인 디렉토리를 가져온다
	if((count = scandir(proc_path, &namelist, *s_filter, alphasort))==-1){
		fprintf(stderr, "%s Directory Scan Error\n", proc_path);
		exit(1);
	}
	proc_count=count;


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

		//PR(IORITY) 구하기
		memset(proc_struct[i].PR, '\0', sizeof(proc_struct[i].PR));
		if(atoi(stoken[17])<=-100){
			strcpy(proc_struct[i].PR, "rt");
		}
		else{
			strcpy(proc_struct[i].PR, stoken[17]);
		}

		//NI(CE) 구하기
		proc_struct[i].NI=atoi(stoken[18]);

		//VIRT 구하기
		proc_struct[i].VIRT=getVmSize(proc_struct[i].PID);

		//RES 구하기
		long long int rss=getRss(proc_struct[i].PID);//추후 사용을 위해
		proc_struct[i].RES = getRss(proc_struct[i].PID);

		//SHR 구하기
		proc_struct[i].SHR=getRssShmem(proc_struct[i].PID);

		//STAT구하기
		proc_struct[i].S = stoken[2][0];
		if(proc_struct[i].S=='R') run++;
		else if(proc_struct[i].S=='D'|'I'|'S') slep++;
		else if(proc_struct[i].S=='T'|'t') stop++;
		else if(proc_struct[i].S=='Z') zom++;

		//uptime 정보 가져오기
		uptime=getUptime();//시스템 부팅후 현재 시각까지의 부팅시간(초)

		//%CPU 계산
		int hertz = (int)sysconf(_SC_CLK_TCK);
		long long int totalTime=atoll(stoken[13])+atoll(stoken[14]+atoll(stoken[15]) + atoll(stoken[16]));
		long long int startTime = atoll(stoken[17]);
		double processUpTime = (double)uptime-((double)startTime/hertz);
		proc_struct[i].CPU = 100*(((double)totalTime/hertz)/processUpTime);

		//memTotal 정보 가져오기
		long long int memTotal = getMemTotal();
		//%MEM 계산
		proc_struct[i].MEM=(double)rss/memTotal*100;

		//TIME 구하기
		//int hertz = (int)sysconf(_SC_CLK_TCK);
		long long int ticTime = atoll(stoken[13]) + (atoll(stoken[14])); 
		long long int minTime= ticTime/10000;
		long long int secTime= ticTime/100;
		ticTime = ticTime-(minTime*10000 + secTime*100);
		memset(proc_struct[i].TIME,  '\0', sizeof(proc_struct[i].TIME));
		sprintf(proc_struct[i].TIME, "%lld:%02lld.%02lld", minTime, secTime, ticTime);	

		//COMMAND가져오기
		memset(proc_struct[i].COMMAND, '\0', MAXLEN);
		strcpy(proc_struct[i].COMMAND, stoken[1]+1);
		proc_struct[i].COMMAND[strlen(proc_struct[i].COMMAND)-1]='\0';
	}

}

int checkinput(){
	char a = ggetch();
	int asci=0;
	if(a=='q') return 1;
	else{
		asci += (int)a;
		char b = ggetch();
		asci += (int)b;
		char c = ggetch();
		asci += (int)c;
		if(asci==183) return 2;
		else if(asci==184) return 3;
	}
	return 0;
}

int main(void){

	fflush(stdout);
	run=0; slep=0; stop=0; zom=0;
	start_line=0;
	save_data();
	ttop_print();

	signal(SIGALRM, sigint_handler);
	
	while(1){		
		alarm(3);
		int ret = checkinput();
		if(ret==1) exit(0); // q 입력 시
		else if(ret==2){
			if(start_line>0) {
				start_line--;
				save_data();
				ttop_print();
			}
			else continue;
			//위 화살표 입력	시
		}
		else if(ret==3){
			if(start_line<proc_count) {
				start_line++;
				save_data();
				ttop_print();
			}
			else continue;
			//아래 화살표 입력시
		}
	}
	return 0;
}
