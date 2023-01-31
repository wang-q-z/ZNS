
#include<string.h>
#include<stdlib.h>
#include <chrono>
#include <iostream>
#include <fcntl.h>
#include <errno.h>

#include <unistd.h>
#include <sys/time.h>


#include <pthread.h>

#include <unordered_map>

#include <libzbd/zbd.h>

#define MAXSIZE 100
#define MAXCOUNT 65535
#define NUM_PTHREADS 1


char num = '0';
long offset = 0;
using Byte = uint8_t;
zbd_zone *zones;
const char* dev_path = "/dev/nullb0";
int flags = O_RDWR;
struct zbd_info info;
int dev_fd = zbd_open(dev_path, flags | O_LARGEFILE, &info);


typedef struct re_value{
    long long w_count;
    double w_time;
    long long r_count;
    double r_time;
}re_value;

re_value re[NUM_PTHREADS];





//read from the workload and get the instruction
int get_instruction(FILE *f, char* ins, int *nwork, int *blocks){
    char readchar[MAXSIZE] = "";
    int i = 0, j = 0;
    int po = 0;
    // standard w 1 20     or   r   1
    if(fgets(readchar,MAXSIZE,f)){
        *ins = readchar[0];
        if(*ins == 'w'){

            for(i = 2; i < MAXSIZE; i++){
                if(readchar[i] == ' '){
                    po = i + 1;
                    break;
                }

            }
            char n_work[20] = "";
            for(j = 0; j < i-2; j++){
                n_work[j] = readchar[2+j];
            }
            *nwork = atoi(n_work);

            for(i = po; i < MAXSIZE; i++) {
                if (readchar[i] == ' ')
                    break;
            }
            char temp[20] = "";
            for(j = 0; j < i-po; j++){
                temp[j] = readchar[po+j];
            }
            *blocks = atoi(temp);
        }else{
            for(i = 2; i < MAXSIZE; i++){
                if(readchar[i] == ' ')
                    break;
            }
            char n_work[20] = "";
            for(j = 0; j < i-2; j++){
                n_work[j] = readchar[2+j];
            }
            *nwork = atoi(n_work);
            *blocks = 0;
        }
    return 0;
    }
    return -1;
}




void update_zone_info(int dev_fd, zbd_zone *zone) {
    auto z = *zone;
    unsigned int nr_zone_1 = 1;
    auto err = zbd_report_zones(dev_fd, z.start, z.len, zbd_report_option::ZBD_RO_ALL, zone, &nr_zone_1);
    if (!err) {
       // std::cout << "refresh info of "<< zone->start <<  "success" << std::endl;
    } else {
       std::cerr << "refresh info of " << zone->start << "error" << std::endl;
    }
}

void do_zone_ctl_operation(int dev_fd, zbd_zone *zone, enum zbd_zone_op op) {
    // ZBD_OP_RESET	  = 0x01,
    // ZBD_OP_OPEN	  = 0x02,
    // ZBD_OP_CLOSE	  = 0x03,
    // ZBD_OP_FINISH  = 0x04
    auto z = *zone;
    auto err = zbd_zones_operation(dev_fd, op, z.start, z.len);
    auto start = zone->start;
    if (!err) {
        update_zone_info(dev_fd, zone);
       // std::cout << "operation " << op <<  "on " << start <<  "success" << std::endl;
    } else {
        std::cerr << "operation " << op <<  "on " << start <<  "error" << std::endl;
    }
}

void do_zone_write(int dev_fd, zbd_zone *zone, const Byte *buf, size_t nbyte) {
    // check the zones[zone_id].wp is 4k alignment
    // nbyte also need to be aligned 4k.
    auto wp = zone->wp;
    if (zone->wp % 0x1000) {
        std::cerr <<"write pointer should aligned to 4k, but got "<< wp << std::endl;
        return;
    } else if (nbyte % 0x1000) {
        std::cerr << "nbyte should aligned to 4k, but got "<< nbyte << std::endl;
        return;
    }
    auto capacity = zone->capacity;
    auto start = zone->start;
    // check space for writing buf
    // std::cout << fmt::format("writing to {} {} bytes", zones[zone_id].wp, nbyte) << std::endl;
    if (zone->wp + nbyte >= zone->start + zone->capacity) {
        std::cerr << "current write pointer is "<< wp <<", capacity is "<< capacity << "\n and zone "<< start <<"have no space for write " << nbyte << "bytes data"<< std::endl;
        return;
    }

    auto err = pwrite(dev_fd, buf, nbyte, zone->wp);
    //auto start = zone->start;
    if (err == -1) {
        std::cerr << "write "<< zone->wp << "error" << std::endl;
    } else {
        // IMPORTANT NOTE: 
        // for the 4k alignment of SSD, wp usually needs to be aligned to 4k, 
        // but wp can not be aligned to 4k when continuous write operations are run
        // 
        // for example: 
        //      when the input is w 1, w 1, w 1, r 1, w 1, w 1, r 1 
        //      ssd will only save the first three write operations, the last two write operations have no effect on ssd
        //      but read operation will give wrong data in the second read operation
        //      MAYBE: the ssd view the first three write operations as only one write operation.
        //      but we SHOULD align 4k for wp
        // 
        // when call zbd_report_zones the updated wp will always be aligned to 4k
        // TODO: handle when err is not aligned to 4k 
        
       //std::cout << "write "<< zone->wp << "success" << std::endl;
        // std::cout << fmt::format("now  zones->wp is {}", zones->wp) << std::endl;
        zone->wp += err;
        // do flush
        // do_flush(dev_fd);
    }
}

void do_zone_read(int dev_fd, zbd_zone *zone, Byte *buf, size_t nbyte, unsigned long long offs, bool print=false) {
    // std::cout << fmt::format("reading from {} {} bytes", zones[zone_id].start, nbyte) << std::endl;
    auto start = zone->start + offs;
    auto err = pread(dev_fd, buf, nbyte, start);
    if (err == -1) {
        std::cerr << "read "<< start <<"error"<< std::endl;
    } else {
        //std::cout <<"read "<< start << "success"<< std::endl;
        }
}



void do_flush(int dev_fd) {
    auto err = fsync(dev_fd);
    if (err == -1) {
        std::cerr << "flush error" << std::endl;
    } else {
        std::cout << "flush success" << std::endl;
    }
}

FILE* open_trace(char num){
    char name[7] = "trace";
    name[5] = num;
    name[6] = '\0';
    std::cout<<"open "<<name<<std::endl;
    FILE *trace = fopen(name,"r");
    return trace;
}




typedef struct zone_table{
    int num; //which work
    int nblock; // how many 4k have
    int start_nzone;//start to write
    unsigned long long start_pt;

    bool is_change;// write full the first zone and change 
    int change_nzone;
    int change_nblock;
}zone_table;


void init_table(zone_table* tab, int numb, int nblocks, int start, unsigned long long pt){
    tab->num = numb;
    tab->nblock = nblocks;
    tab->start_nzone = start;
    tab->start_pt = pt;
    tab->is_change = 0;
    tab->change_nzone = 0;
    tab->change_nblock = 0;
}



void *doo_io(void *args){
    char count = *(char *)args;

    FILE *trace = open_trace(count);

    std::unordered_map<int,zone_table> work_map;

    std::unordered_map<int,int> id_table;
    int zone_id = int(count - '0');
    id_table[zone_id] = 1;

    int num = 0;
    char ins;
    int nblock = 0;

    int w_count = 0;

    size_t nbyte = 0x1000;
    Byte *buf = (Byte*) malloc(nbyte);
    // example data for writing
    //std::cout<<"is there ?"<<std::endl;
    for (int i = 0; i < nbyte; i++) {
        buf[i] = 2;
    }
    if (buf == nullptr) {
        std::cerr << "malloc buf error!" << std::endl;
    }

    //std::time_t start, end;
    long long rcount = 0;
    long long wcount = 0;
    double rtime = 0;
    double wtime = 0;

    struct timeval t1,t2;

    while(get_instruction(trace, &ins, &num, &nblock)!= -1)
    {
        if(ins == 'w'){

            if(num == 1){
                do_zone_ctl_operation(dev_fd, zones + zone_id, ZBD_OP_RESET);
                update_zone_info(dev_fd, zones + zone_id);
            }

            if(work_map.find(num) == work_map.end()) {// new work
                zone_table tab;
                init_table(&tab, num, nblock, zone_id,(zones+zone_id)->wp);
                work_map[num] = tab;
                //std::cout<<"table info : "<<"start: "<<tab.start<<" num: "<<tab.num<<" nblocks: "<<tab.nblock<<std::endl;
            }
            gettimeofday(&t1,NULL);
            for(int i = 0; i < nblock; i++){
                do_zone_write(dev_fd, zones + zone_id, buf, nbyte);
                w_count++;

                wcount++;

                if(w_count == MAXCOUNT ){

                    zone_id += NUM_PTHREADS;

                    work_map[num].is_change = 1;
                    work_map[num].change_nblock = work_map[num].nblock - i - 1;
                    work_map[num].change_nzone = zone_id;
                    i = 0;
                    w_count = 0;
                    nblock = work_map[num].change_nblock;
                    do_zone_ctl_operation(dev_fd, zones + zone_id, ZBD_OP_RESET);
                    update_zone_info(dev_fd, zones + zone_id);
                }
            }

            gettimeofday(&t2,NULL);
            wtime += ((t2.tv_sec - t1.tv_sec) + (double)(t2.tv_usec - t1.tv_usec)/1000000.0);
            //std::cout<<"Threads"<<count<<"  table info : "<<" num: "<<work_map[num].num<<"start: "<<work_map[num].start_nzone<<" nblocks: "<<work_map[num].nblock<<std::endl;
            //std::cout<<"Threads"<<count<<"  table info : "<<" num: "<<work_map[num].num<<"is_change: "<<work_map[num].is_change<<" change_start: "<<work_map[num].change_nzone<<"res_block"<<work_map[num].change_nblock<<std::endl;

        }else{
            int read_nblock = work_map[num].nblock;
            bool is_change = work_map[num].is_change;
            int start_zone_id = work_map[num].start_nzone;
            int change_nblock = work_map[num].change_nblock;
            unsigned long long offs = work_map[num].start_pt - (zones+start_zone_id)->start;

            gettimeofday(&t1,NULL);
            if(is_change == false){
                for(int i = 0; i < read_nblock; i++){
                    Byte *r_buf = (Byte*) malloc(nbyte);
                    do_zone_read(dev_fd, zones+start_zone_id, r_buf, nbyte, offs, false);
                    rcount++;
                    offs += 0x1000;
                    free(r_buf);
                }
            }else{
                int before_change_nblock = read_nblock - change_nblock;
                for(int i = 0; i < before_change_nblock; i++){
                    Byte *r_buf = (Byte*) malloc(nbyte);
                    do_zone_read(dev_fd, zones+start_zone_id, r_buf, nbyte, offs, false);
                    rcount++;
                    offs += 0x1000;
                    free(r_buf);
                }
                offs = 0;
                for(int i = 0; i < change_nblock; i++){
                    Byte *r_buf = (Byte*) malloc(nbyte);
                    do_zone_read(dev_fd, zones+work_map[num].change_nzone, r_buf, nbyte, offs, false);
                    rcount++;
                    offs += 0x1000;
                    free(r_buf);
                }
            }
            gettimeofday(&t2,NULL);
            rtime +=((t2.tv_sec - t1.tv_sec) + (double)(t2.tv_usec - t1.tv_usec)/1000000.0);

            }

        }

        re[int(count-'0')].w_count = wcount;
        re[int(count-'0')].w_time = wtime ;
        re[int(count-'0')].r_count = rcount;
        re[int(count-'0')].r_time = rtime ;
}



void start_to_threads(int thread_count)
{
    pthread_t *g_tid = NULL;
    g_tid = (pthread_t*)malloc(sizeof(pthread_t*)*thread_count);

    char count[thread_count];
    if(g_tid == NULL)
    {
        perror("There is no more memory\n");
        exit(errno);
    }
    //every thread has own workloads
    for(int i = 0; i < thread_count; i++)
    {
        count[i] = '0'+i;
    }

    int i = 0;
    for (i = 0; i < thread_count; i++)
    {
        //
        
        int ret = pthread_create(&g_tid[i],NULL,doo_io,&count[i]);
           
        if(ret != 0)
        {
            perror("error creating threads.\n");
            if (ret == EAGAIN) {
                perror("not enough system resources.\n");
            }
            exit(errno);            
        }
        
    }
    

     
    for (i = 0; i < thread_count; i++) {
       if (pthread_join(g_tid[i], NULL) != 0)
           perror("thread wait error.\n");
    
   }

    if (g_tid != NULL)
        free(g_tid);
}




int main(int argc, char *argv[]){

    if (dev_fd < 0) {
        std::cerr << "Open "<< dev_path << " failed" << std::endl;
        return 1;
    }


    unsigned int nr_zones;
    int ret = zbd_list_zones(dev_fd, 0, 0, zbd_report_option::ZBD_RO_ALL, &zones, &nr_zones);



    start_to_threads(NUM_PTHREADS);

    for(int i = 0; i < NUM_PTHREADS; i++){
        std::cout<<"Thread "<<i<<" write counts "<<re[i].w_count /256 <<"Mb write time "<<re[i].w_time<<\
        "s average rate: "<<(re[i].w_count) / (1024*256*re[i].w_time)<<"Gb/s"<<std::endl;

        std::cout<<"Thread "<<i<<" read counts "<<re[i].r_count / 256 <<"Mb read time "<<re[i].r_time<<\
        "s average rate: "<<(re[i].r_count) / (1024*256*re[i].r_time)<<"Gb/s"<<std::endl;

    }

    return 0;
}
