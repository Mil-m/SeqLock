#include <stdio.h>
#include <stdlib.h>
#ifdef _MSC_VER
    #include <windows.h>
    #include <process.h>
#elif defined(__GNUC__)
    #include <pthread.h>
    #include <unistd.h>
#endif
#define NUM_THREADS	4

typedef struct { int empty; } spinlock_t;

typedef struct {
    volatile unsigned sequence;
    spinlock_t lock;
} seqlock_t;
//sequence - порядковый номер
//lock - блокировка
//AT&T => источник, приёмник

volatile int run_test = 0;

void spin_lock(spinlock_t* locked)
{
    int perem = 1;
    for (;;) {
        asm (
            "xchg %0,%1\n\t"
            : "=r" (perem)
            : "m" (*locked), "0" (perem));
        if (perem == 0) {
            break;
        }
    }
    return;
}

void write_lock(seqlock_t *sl)
{
    spin_lock(&sl->lock);
    sl->sequence++;
    return;
}

void spin_unlock(spinlock_t* locked)
{
    int perem = 0;
    asm volatile (
        "xchg %0,%1\n\t"
        : "=r" (perem)
        : "m" (*locked), "0" (perem));
    return;
}

void write_unlock(seqlock_t *sl)
{
    sl->sequence++;
    spin_unlock(&sl->lock);
    return;
}

//----------------------------------------------------------------------------------reader

const int n = 10;
seqlock_t perem;
int mas[10];
const int SUMMA = 45;
int ERR = 0;

int function_reading(int n)
{
    int i;
    int sum = 0;
    for (i=0; i<n; i++) {
        sum += mas[i];
    }
    return sum;
}

inline int prov_bit(seqlock_t *sl) {
    int count = sl->sequence;
    while (count & 1) {
        count = sl->sequence;
    }
    return count;
}

inline int prov_writer(int count, seqlock_t *sl) {
    if (count != sl->sequence) {
        return 1;   //Пришёл писатель => идём на повтор цикла
    } else {
        return 0;   //Иначе выход из do-while и повтор цикла по j
    }
}

void* read_seqlock()
{
    int count, sum; //, j;

    while (!run_test) {}
//    for (j=0; j<10000000; j++) {
    while ( run_test != 2 ) {
        do {
            count = prov_bit(&perem);    //Проверка 1-го бита. Если да, то нечётное - ждём, когда писатель закончит записывать
            sum = function_reading(n);                //Чётное => писатель закончил записывать => считываем
        } while (prov_writer(count, &perem));   //Ждём писателя. Цикл, пока () == 1
        if (sum != SUMMA) {
            ERR++;
        }
    }
    return NULL;
}

void fill( void )
{
    int i, t;
    t = rand()%10;
    for (i=0; i<10; i++) {
        mas[t] = i;
        t++;
        if (t >= 10) {
            t = 0;
        }
    }
}

void* write_seqlock()
{
//    int j;
    while (!run_test) {}
//    for (j=0; j<10000000; j++) {
    while (run_test!=2) {
        write_lock(&perem);
        fill();
        write_unlock(&perem);
    }
    return NULL;
}

int main()
{
    int i;
    pthread_t thread[NUM_THREADS];
    fill();
    for (i=0;i<NUM_THREADS;i+=2 ) {
        pthread_create(&thread[i], NULL, &write_seqlock, NULL);
        pthread_create(&thread[i+1], NULL, &read_seqlock, NULL);
    }
    sleep(1);
    run_test = 1;
    sleep(15);
    run_test = 2;
    for (i=0;i<NUM_THREADS;i++) pthread_join(thread[i], NULL);
    for (i=0; i<10; i++) {
        printf("%d\n\t", mas[i]);
    }
    printf("%d\n\t", ERR);
    return 0;
}

/*Потоки создаются функцией pthread_create(3), определенной в заголовочном файле <pthread.h>.
1 параметр ф-ии - указатель на переменную типа pthread_t, которая служит идентификатором создаваемого потока.
2 параметр - указатель на переменную типа pthread_attr_t, используется для передачи атрибутов потока.
3 параметр - адрес функции потока.
4 параметр имеет тип void *. Может использоваться для передачи значения, возвращаемого функцией потока.
Вскоре после вызова pthread_create() функция потока будет запущена на выполнение параллельно с другими потоками программы.

int pthread_barrier_init(pthread_barrier_t *restrict barrier,
const pthread_barrierattr_t *restrict attr, unsigned count);

void do_read() {
    do {
        while ((count = lock->counter) & 1);
        [do read stuffs...]
    } while (lock->counter != count);
}*/
