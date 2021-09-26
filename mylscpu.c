#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define MAXLEN 1024

typedef struct mycpu{
	//필수
	char Vendor_ID[100];
	char Model_name[MAXLEN];
	char CPU_MHz[100];
	char cache_size[3][100];
	//옵션
	char address_sizes[MAXLEN];
	char CPUs[10];
	char CPU_family[100];
	char model[100];
	char stepping[10];
	char bogoMIPS[100];
	char flags[MAXLEN];
}mycpu;

mycpu cpu_struct;
char path[MAXLEN];
FILE *cpu_fp;

void getCPUinfo();
void getcacheinfo();
char *getString(char *str, int len);

int main(void){
	getCPUinfo();
	
	printf("%-32s%s\n", "Address sizes:", cpu_struct.address_sizes);
	printf("%-32s%s\n", "CPU(s):", cpu_struct.CPUs);
	printf("%-32s%s\n", "Vendor ID:", cpu_struct.Vendor_ID);
	printf("%-32s%s\n", "CPU family:", cpu_struct.CPU_family);
	printf("%-32s%s\n", "Model:", cpu_struct.model);
	printf("%-32s%s\n", "Model Name:", cpu_struct.Model_name);
	printf("%-32s%s\n", "Stepping:", cpu_struct.stepping);
	printf("%-32s%s\n", "CPU MHz:", cpu_struct.CPU_MHz);
	printf("%-32s%s\n", "BogoMIPS:", cpu_struct.bogoMIPS);
	printf("%-33s%s\n", "L1d cache:", cpu_struct.cache_size[0]);
	printf("%-33s%s\n", "L1i cache:", cpu_struct.cache_size[1]);
	printf("%-33s%s\n", "L2 cache:", cpu_struct.cache_size[2]);
	printf("%-32s%s\n", "flags:", cpu_struct.flags);

	return 0;
}
char * getString(char *str, int len){
	char line_info[MAXLEN];
	char *ret_str = NULL;
	char *next_str = NULL;
	int check = 0;
	memset(path, '\0', MAXLEN);
	memset(line_info, '\0', MAXLEN);
	strcpy(path, "/proc/cpuinfo");

	if((cpu_fp = fopen(path, "r")) == NULL){
		fprintf(stderr, "cpuinfo open error\n");
		exit(1);
	}
	while(fgets(line_info, MAXLEN, cpu_fp) != NULL){
		if(strncmp(line_info, str, len) == 0){
			check = 1;
			break;
		}
		else{
			memset(line_info, '\0', MAXLEN);
			continue;
		}
	}

	if(check == 1){
		fclose(cpu_fp);
		ret_str = strtok(line_info, ":");
		ret_str = strtok(NULL, "\n");
		return ret_str;
	}
	else{
		fclose(cpu_fp);
		return 0;
	}

}

void getcacheinfo(){
	FILE *cache_fp = NULL;
	int cpus = atoi(cpu_struct.CPUs);
	char cache_path[MAXLEN];
	char *next_str[3] = {"/cache/index0/size", "/cache/index1/size", "/cache/index2/size"};
	char cpu_num[3];
	char ch_cache[3][10];
	int cache[3] = {0,};

	strcpy(cache_path, "/sys/devices/system/cpu/cpu");

	for(int i = 0;i < cpus;i++){
		sprintf(cpu_num, "%d", i);
		strcat(cache_path, cpu_num);// /sys/devices/system/cpu/cpu#
		for(int j = 0;j < 3; j++){
			//cache 경로 초기화
			memset(cache_path, '\0', MAXLEN);
			strcpy(cache_path, "/sys/devices/system/cpu/cpu");
			strcat(cache_path, cpu_num);

			//cache index 경로 붙이기
			strcat(cache_path, next_str[j]);

			//cache size 담아올 공간 초기화
			memset(ch_cache[j], '\0', 10);

			//파일 열어서 갖고옴
			cache_fp = fopen(cache_path, "r");
			fgets(ch_cache[j], 100, cache_fp);
			cache[j] += atoi(ch_cache[j]);
		}
		fclose(cache_fp);
	}

	sprintf(cpu_struct.cache_size[0], "%d", cache[0]);
	strcat(cpu_struct.cache_size[0], "kiB");
	sprintf(cpu_struct.cache_size[1], "%d", cache[1]);
	strcat(cpu_struct.cache_size[1], "KiB");
	sprintf(cpu_struct.cache_size[2], "%d", cache[2]/1024);
	strcat(cpu_struct.cache_size[2], "MiB");

}

void getCPUinfo(){
	char *vendor_id, *model_name, *cpu_mhz;
	char *address_sizes, *cpus, *cpu_family, *model, *stepping, *bogomips, *flags;

	/****************ESSENTIAL*****************/
	vendor_id = getString("vendor_id", 9);
	strcpy(cpu_struct.Vendor_ID, vendor_id);

	model_name = getString("model name", 10);
	strcpy(cpu_struct.Model_name, model_name);

	cpu_mhz = getString("cpu MHz", 7);
	strcpy(cpu_struct.CPU_MHz, cpu_mhz);

	/******************OPTION*********************/
	address_sizes = getString("address sizes", 13);
	strcpy(cpu_struct.address_sizes, address_sizes);

	cpus = getString("cpu cores", 9);
	strcpy(cpu_struct.CPUs, cpus);

	cpu_family = getString("cpu family", 10);
	strcpy(cpu_struct.CPU_family, cpu_family);

	model = getString("model", 5);
	strcpy(cpu_struct.model, model);

	stepping = getString("stepping", 8);
	strcpy(cpu_struct.stepping, stepping);

	bogomips = getString("bogomips", 8);
	strcpy(cpu_struct.bogoMIPS, bogomips);

	flags = getString("flags", 5);
	strcpy(cpu_struct.flags, flags);

	getcacheinfo();
}
