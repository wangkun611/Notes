#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>


int exit_flag=0;
void* loop(void *param) {
    for(;!exit_flag;);

    return NULL;
}

void sighandler(int sig) {
    printf("Exit\n");
    exit_flag=1;
}

int main(int argc, char**argv) {
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'h') {
	printf("Usage: man [OPTION...] [number]\n");
	printf("The cpu usage rate of the process is [number]x100%\n\n");
	printf("    -h\t\tgive this help list\n");

	return 0;
    }
     
    signal(SIGINT, sighandler);

    int num = 1;
    if (argc > 1) {
	num = atoi(argv[1]);    
    }

    pthread_t pId;
    for (int i = 1; i < num; ++i) {
        pthread_create(&pId, NULL, loop, NULL);
    }    
    for(;!exit_flag;);
    return 0;
}

