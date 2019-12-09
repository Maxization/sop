#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))

void usage(char *name){
        fprintf(stderr,"USAGE: %s TODO\n",name);
        exit(EXIT_FAILURE);
}

typedef unsigned int UINT;
typedef struct args {
	pthread_t tid;
	int t;
    int n;
    int* tab;
    int* seeds;
    pthread_mutex_t *mxTab;
} args_t;

typedef struct argsR {
	pthread_t tid;
    int n;
    int state; // = 1 <-> inactive 
    pthread_mutex_t *mxState;
    int* tab;
    UINT seed;
    pthread_mutex_t *mxTab;
} argsR_t;

void ChangeState(void* arg)
{
    argsR_t* args = arg;
    pthread_mutex_lock(args->mxState);
    args->state = 1;
    pthread_mutex_unlock(args->mxState);
} 

void* R_work(void *voidPtr) 
{
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    argsR_t* argsR = voidPtr;
    int a,b;
    printf("R PID: %ld\n",argsR->tid);
    while(1)
    {
        a = rand_r(&argsR->seed) % (argsR->n-1);
        b = rand_r(&argsR->seed) %(argsR->n - a - 1) + a + 1;
        //printf("N: %d, A: %d, B: %d\n",argsR->n,a,b);
        int ind = a;

        pthread_mutex_lock(argsR->mxTab);
        int maks = argsR->tab[a];
        for(int i=a+1;i<=b;i++)
        {
            if(maks<argsR->tab[i])
            {
                ind = i;
                maks = argsR->tab[i]; 
            }
        }
        int tmp = argsR->tab[b];
        argsR->tab[b] = maks;
        argsR->tab[ind] = tmp;
        pthread_mutex_unlock(argsR->mxTab);

        int msec = rand_r(&argsR->seed) % 1001 + 500;
        struct timespec tk = {msec/1000, msec % 1000};


        pthread_cleanup_push(ChangeState,argsR);
        nanosleep(&tk,NULL);
        pthread_cleanup_pop(0);
    }
    return NULL;
}

void* P_work(void *voidPtr) 
{
    args_t* args = voidPtr;
    argsR_t* argsR = (argsR_t*)malloc(sizeof(argsR_t)*args->t);
    pthread_mutex_t* mxStates = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)*args->t);
    if(argsR == NULL || mxStates == NULL) ERR("malloc");
    for(int i=0;i<args->t;i++)
    {
        if(pthread_mutex_init(&mxStates[i], NULL))ERR("Couldn't initialize mutex!");
    }
    printf("P TID: %ld\n",args->tid);
    
    for(int i=0;i<args->t;i++)
    {
        argsR[i].mxTab = args->mxTab;
        argsR[i].n = args->n;
        argsR[i].tab = args->tab;
        argsR[i].seed = args->seeds[i];
        argsR[i].state=0;
        argsR[i].mxState = &mxStates[i];
    }

    for(int i=0;i<args->t;i++)
    {
        int err = pthread_create(&argsR[i].tid, NULL, R_work, &argsR[i]);
        if (err != 0) ERR("Couldn't create thread"); 
    }

    while(1)
    {
        pthread_mutex_lock(args->mxTab);
        for(int i=0;i<args->n;i++)
        {
            printf("%d ",args->tab[i]);
        }
        printf("\n");
        pthread_mutex_unlock(args->mxTab);

        int ile=0;
        for(int i=0;i<args->t;i++)
        {
            pthread_mutex_lock(argsR[i].mxState);
            if(argsR[i].state == 1) ile++;
            pthread_mutex_unlock(argsR[i].mxState);
        }
        if(ile == args->t) break;
        sleep(2);
    }
    
    for(int i=0;i<args->t;i++)
    {
        int err = pthread_join(argsR[i].tid, NULL);
        if (err != 0) ERR("Can't join with a thread");
    }

    free(argsR);
    for (int i = 0; i < args->t; i++) pthread_mutex_destroy(&mxStates[i]);
    free(mxStates);
    return NULL;
}

int main(int argc, char** argv)
{
    int n = atoi(argv[1]);
    int t = atoi(argv[2]);
    if(n<=1 || t <=1) usage(argv[0]);
    int* tab = (int*)malloc(sizeof(int)*n);
    int* seeds = (int*)malloc(sizeof(int)*t);
    if(tab == NULL || seeds == NULL) ERR("malloc");

    srand(time(NULL));
    for(int i=0;i<t;i++)
    {
        seeds[i] = rand();
    }
    pthread_mutex_t mxTab = PTHREAD_MUTEX_INITIALIZER;
    args_t args;
    args.n=n;
    args.t = t;
    args.tab = tab;
    args.seeds = seeds;
    args.mxTab = &mxTab;

    srand(time(NULL));
    for(int i=0;i<n;i++)
    {
        tab[i] = rand()%513;
    }

    int err = pthread_create(&args.tid, NULL, P_work, &args);
    if (err != 0) ERR("Couldn't create thread");

    err = pthread_join(args.tid, NULL);
    if (err != 0) ERR("Can't join with a thread");

    free(tab);
	return(EXIT_SUCCESS);
}
