#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define N 3
#define M 6

sem_t r,b,t;
pthread_mutex_t m;
int x,y;

void* f(void* a) 
{
    int i = *(int*)a;
    int c = i%2;
    
    printf("%d (%s) came\n",i,c?"BLUE":"RED");
    
    if(c) { sem_wait(&r); y++; sem_post(&b); }
    else  { sem_wait(&b); x++; sem_post(&r); }
    
    sem_wait(&t);
    printf("%d (%s) sits\n",i,c?"BLUE":"RED");
    sleep(1);
    sem_post(&t);
    printf("%d (%s) left\n",i,c?"BLUE":"RED");
    
    pthread_mutex_lock(&m);
    if(c) y--; else x--;
    pthread_mutex_unlock(&m);
    
    if(c) sem_post(&r); else sem_post(&b);
    
    return NULL;
}

int main() {
    pthread_t p[M];
    int a[M];
    
    sem_init(&r,0,0);
    sem_init(&b,0,0);
    sem_init(&t,0,N);
    pthread_mutex_init(&m,NULL);
    
    for(int i=0;i<M;i++) {
        a[i]=i;
        pthread_create(&p[i],NULL,f,&a[i]);
        sleep(1);
    }
    
    for(int i=0;i<M;i++) pthread_join(p[i],NULL);
    
    sem_destroy(&r);
    sem_destroy(&b);
    sem_destroy(&t);
    pthread_mutex_destroy(&m);
    
    return 0;
}
