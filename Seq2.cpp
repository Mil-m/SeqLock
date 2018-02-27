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
    #ifdef _MSC_VER
    for (;;) {
        __asm {	// __asm__
	    mov		eax, perem
            mov		ebx, locked
            xchg    eax, dword ptr [ebx]
            mov     perem, eax
            mov     locked, ebx
        }
        if (perem == 0) {
            break;
        }
    }
    #elif defined(__GNUC__)
    for (;;) {
        asm volatile (
            "xchg %0,%1\n\t"
            : "=r" (perem)
            : "m" (*locked), "0" (perem));
        if (perem == 0) {
            break;
        }
    }
    #endif
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
    #ifdef _MSC_VER
    locked->empty = 0;
	#elif defined(__GNUC__)
    asm volatile (
        "xchg %0,%1\n\t"
        : "=r" (perem)
        : "m" (*locked), "0" (perem));
    #endif
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
void* buf;

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
    int count = (int)sl->sequence;
    while (count & 1) {
        count = (int)sl->sequence;
    }
    return count;
}

inline int prov_writer(int count, seqlock_t *sl) {
    if ((unsigned)count != sl->sequence) {
        return 1;   //Пришёл писатель => идём на повтор цикла
    } else {
        return 0;   //Иначе выход из do-while и повтор цикла по j
    }
}

#ifndef _MSC_VER
	void *
#else
	unsigned __stdcall
#endif
	read_seqlock(void* buf)
{
    volatile int count, sum; //, j;
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

#ifndef _MSC_VER
	void *
#else
	unsigned __stdcall
#endif
	write_seqlock(void* buf)
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
    #ifdef _MSC_VER
        HANDLE thread[NUM_THREADS];
    #elif defined(__GNUC__)
        pthread_t thread[NUM_THREADS];
    #else
        printf("error\n");
    #endif

    fill();

    for (i=0;i<NUM_THREADS;i+=2 ) {
        #ifdef _MSC_VER
            thread[i] = (HANDLE)_beginthreadex(NULL, 0, &write_seqlock, NULL, 0, NULL); Sleep(0);
        #elif defined(__GNUC__)
            pthread_create(&thread[i], NULL, &write_seqlock, NULL);
        #else
            printf("error\n");
        #endif

        #ifdef _MSC_VER
            thread[i+1] = (HANDLE)_beginthreadex(NULL, 0, &read_seqlock, NULL, 0, NULL); Sleep(0);
        #elif defined(__GNUC__)
            pthread_create(&thread[i+1], NULL, &read_seqlock, NULL);
        #else
            printf("error\n");
        #endif
    }

    #ifdef _MSC_VER
        Sleep(1000);
    #elif defined(__GNUC__)
        sleep(1);
    #endif

    run_test = 1;

    #ifdef _MSC_VER
        Sleep(15000);
    #elif defined(__GNUC__)
        sleep(15);
    #endif
// любая из двух конструкций ниже
//     #ifndef __GNUC__
//     #if !defined(__GNUC__)
//         Sleep(15000);
//     #else
//         sleep(15);
//     #endif
	
// #define A
// #define B
// #ifdef C
//	#undef C
// #endif
// #if defined(A) || defined(B) && !defined(C)
// ....
// #endif
	
// #define X 0
// #define Y 1
// #ifdef X
// #if X
// ... не компилируется, т.к. X==0
// #else
// ...
// #endif
// #endif

    run_test = 2;

    #ifdef _MSC_VER
        WaitForMultipleObjects(NUM_THREADS - 1, thread, TRUE, INFINITE);
    #endif

    for (i=0;i<NUM_THREADS;i++) {
        #ifdef _MSC_VER
            CloseHandle(thread[i]);
        #elif defined(_GNUC_)
            pthread_join(thread[i], NULL);
        #endif
    }

    for (i=0; i<10; i++) {
        printf("%d\n\t", mas[i]);
    }
    printf("%d\n\t", ERR);

    getchar();
/*	#ifdef _MSC_VER
		system("pause");			//много тактов
	#elif defined(_GNUC_)
		system("read")
	#endif*/
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
