// 郭俊甫 521021910522
#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef struct cacheline{
    int valid;
    long tag;
    int timeStamp;
}Cacheline, *Cacheblock, **Cache;

extern char* optarg;
extern int optind;
extern int opterr;
extern int optopt;
int hit_count = 0;
int miss_count = 0;
int eviction_count = 0;
int S, s, B, b, E;
Cache cache;
char t[100];



void deal(unsigned long index, unsigned long tag){
    // printf("%lx %lx ", tag, index);
    // printf("tag:");
    // for(int i = 0;i < E;++i){
    //     printf("%lx ", cache[index][i].tag);
    // }

    // 增加时间戳
    for(int i = 0;i < E;++i){
        ++cache[index][i].timeStamp;
    }
    for(int i = 0;i < E;++i){
        // printf("tag:%u\t", cache[index][i].tag);
        
        // 命中
        if(cache[index][i].tag == tag){
            if(cache[index][i].valid){
                cache[index][i].timeStamp = 0; // 重置时间戳
                ++hit_count;
                printf("hit ");
                // printf("%lx %lx hit\t", index, tag);
                return;
            }
        }
    }

    // 不命中但无冲突，同时统计时间戳最大的cacheline
    int max = 0;
    int max_index = 0;
    for(int i = 0;i < E;++i){
        if(!cache[index][i].valid){
            cache[index][i].valid = 1;
            cache[index][i].tag = tag;
            cache[index][i].timeStamp = 0;
            ++miss_count;
            printf("miss ");
            // printf("%lx %lx miss\t", index, tag);
            // printf("tag:");
            // for(int i = 0;i < E;++i){
            //     printf("%lx ", cache[index][i].tag);
            // }
            return;
        }
        if(cache[index][i].timeStamp > max){
            max = cache[index][i].timeStamp;
            max_index = i;
        }
    }

    // 不命中且冲突
    cache[index][max_index].tag = tag;
    cache[index][max_index].timeStamp = 0;
    ++miss_count;
    printf("miss eviction ");
    // printf("%lx %lx miss\t", index, tag);
    // printf("%lx %lx eviction\t", index, tag);
    ++eviction_count;
    // printf("tag:");
    // for(int i = 0;i < E;++i){
    //     printf("%lx ", cache[index][i].tag);
    // }
}

int main(int argc, char *argv[])
{
    char opt;
    // printf("1");
    // fflush(stdout);
    // 解析命令行参数
    while((opt = getopt(argc, argv, "s:E:b:t:")) != -1){
        switch (opt)
        {
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            strcpy(t, optarg);
            break;
        default:
            break;
        }
    }

    // 初始化cache
    S = 1 << s;
    B = 1 << b;
    cache = (Cache)malloc(sizeof(Cacheblock) * S);
    // printf("%d %d %d %s\n", s, E, b, t);
    // fflush(stdout);
    for(int i = 0;i < S;++i){
        cache[i] = (Cacheblock)malloc(sizeof(Cacheline) * E);
        for(int j = 0;j < E;++j){
            cache[i][j].valid = 0;
            cache[i][j].timeStamp = 0;
            cache[i][j].tag = 0;
        }
    }
    // printf("3\n");
    // fflush(stdout);

    // 解析输入
    FILE *fp;
    fp = fopen(t, "r");
    if(fp == NULL){
        printf("failed!\n");
        return -1;
    }
    char method;
    unsigned long address;
    int bytes;
    unsigned long index;
    unsigned long tag;
    // printf("4\n");
    // fflush(stdout);
    while (fscanf(fp, " %c %lx,%d", &method, &address, &bytes) > 0)
    {
        index = (address >> b) & ((-1U >> (32 - s)));
        tag = (address >> (b + s)) & ((-1U >> (s + b)));
        // printf("%x\n", address);
        // printf("%u\n", b + s);
        // printf("%u\n", address >> (b+s));
        // printf("%x\n", (-1U >> 6));
        if(method != 'I')
            printf("\n%c %lx,%d ", method, address, bytes);
        // fflush(stdout);
        switch (method)
        {
        case 'I':
            break;
        case 'M': // M相当于操作两次
            deal(index, tag);
        case 'L': // L和S实质上是同种操作
        case 'S':
            deal(index, tag);
        default:
            break;
        }
    }
    // printf("5\n");
    // fflush(stdout);
    fclose(fp);
    for(int i = 0;i < S;++i)
        free(cache[i]);
    free(cache);
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
