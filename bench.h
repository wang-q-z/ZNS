#ifndef BENCH_H
#define BENCH_H

#include <string.h>
#include <stdlib.h>
#include <chrono>
#include <iostream>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

#include <pthread.h>
#include <unordered_map>
#include <libzbd/zbd.h>
#include <mutex>
#include <list>
#include <thread>
#include<vector>

#define MAXZONE 10

#define debug  std::cout<<"here are "<<std::endl;
int max_block;

typedef struct zone_table
{
    int zone_id_;
    int off_block;
};

//全局的zns配置表
typedef struct znsconf {
    int znsnum[MAXZONE];//保存现在开启的zone_Id  可能会修改类型，
    //hashtable 用来映射zone到block表
    std::unordered_map<int, int> blockmap;
    //用户
    //std::unordered_map<int, std::list<zone_table>*> z_table;
    //std::mutex confmtx;//修改conf时的锁
}znsconf;


//这一步不需要加锁 
void init_conf(znsconf *conf)
{
    //conf->blockmap = new std::unordered_map<int, int>(MAXZONE);
    //std::cout<<"he"<<std::endl;
    for (int i = 0; i < MAXZONE; ++i)
    {

        conf->znsnum[i] = i;              //初始的zone_id暂时设为0-9
        conf->blockmap[i] = 0;
    }

}

//返回偏移 zone_id
int allocate_block(int thread_id, int *zone_id, znsconf *conf)
{
    //一般先从thread_id对应的zone取
    int off = conf->blockmap[thread_id];
    std::cout<<thread_id<<" "<<conf->blockmap[thread_id]<<std::endl;
    if(off < max_block)
    {
        conf->blockmap[thread_id]++;
        *zone_id = thread_id;
       // std::cout<<"zone_id: "<<*zone_id<<"off_block: "<<off<<std::endl;
        return off;
    }
    else{
        return -1;
    }
}



#endif
