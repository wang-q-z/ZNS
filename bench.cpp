#include"bench.h"


#define MAXSIZE 100
#define NUM_PTHREADS 2
#define size_block 0x1000
#define MAX_FILE_NAME_LENGTH 1024

#define debug  std::cout<<"here are "<<std::endl;


using Byte = uint8_t;

//const char* dev_path = "/dev/nullb0";
int flags = O_RDWR;
int num_thread;

char filename[MAX_FILE_NAME_LENGTH] = "testfile.tmp";

typedef struct re_value{
    long long w_count;
    double w_time;
    long long r_count;
    double r_time;
}re_value;

void usage(void)
{
    fprintf(stderr,
        "\n"
 
        "SYNOPSIS\n"
        "                [ -b num_block_perzone][ -f filename ] \n"
        "                [ -p num_thread ]  \n"
        "\n"
    );
}

void get_options(int argc, char **argv, int *dev_fd, struct zbd_info *info)
{
    int opt;
    while ((opt = getopt(argc, argv, "b:f:p:")) != -1) {
        switch (opt) {
        case 'b':
            max_block = atoi(optarg);
            if (max_block <= 0) {
                printf("incorrect value %s. for - b <max_block_count>\n", optarg);
                exit(-1);
            }
            //std::cout<<max_block<<std::endl;
            break;
        case 'f':
            strncpy(filename, optarg, MAX_FILE_NAME_LENGTH - 1);
            *dev_fd = zbd_open(filename, flags | O_LARGEFILE, info);
            //std::cout<<filename<<" "<<*dev_fd<<std::endl;
            if(*dev_fd < 0)
            {
                printf("Failed to open %s\n", filename);
                exit(-2);
            } 
            break;
        case 'p':
            num_thread = atoi(optarg);
                if (num_thread <= 0) {
                printf("incorrect value %s. for - p <threads_count>\n", optarg);
                exit(-3);
            }
        break;
        }
    }
    
}

//read from the workload and get the instruction
int get_instruction(FILE *f, char* ins, int *nwork, int *blocks){
    char* readchar = new char[MAXSIZE];
    for(int i = 0; i < MAXSIZE; i++)
    {
        readchar[i] = '0';
    }
    int i = 0, j = 0;
    int po = 0;
    // standard w 1 20     or   r   1
    if(fgets(readchar,MAXSIZE,f) != NULL){
        *ins = readchar[0];
        if(*ins == 'w'){
            char *tnum = new char[10];
            for(int i = 0 ; i < 10; i++)
                tnum[i] = '0';
            int j = 0;
            for(int i = 1; i < MAXSIZE; i++)
            {
                if(readchar[i] == ' ')
                {
                    po = i;
                    break;
                }
   
                tnum[j++] = readchar[i];

            }
            tnum[j] = '\0';
            *nwork = atoi(tnum);
            delete[] tnum;
            *blocks = readchar[po+1] - '0';
        }else{
            char *tnum = new char[10];
            for(int i = 0 ; i < 10; i++)
            tnum[i] = '0';
            int j = 0;
            for(int i = 1; i < MAXSIZE; i++)
            {
                if(readchar[i] == '\0')
                    break;
                tnum[j++] = readchar[i];
            }
            tnum[j] = '\0';
            *nwork = atoi(tnum);
            delete[] tnum;
            *blocks = 0;
        }
    delete[] readchar;
    return 0;
    }
    delete[] readchar;
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
        //std::cout << "flush success" << std::endl;
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



void doo_io(int count, re_value *re,zbd_zone * zones, int dev_fd){

    //std::cout<<count<<std::endl;
    char c = '0' + count;
    //std::cout<<c<<std::endl;    
    FILE *trace = open_trace(c);
    int thread_id = count;

    std::unordered_map<int, std::list<zone_table>*> z_table;

    int num = 0;
    char ins = '0';
    int nblock = 0;

    int allo_block = 0;

    int w_count = 0;

    size_t nbyte = 0x1000;
    Byte *buf = (Byte*) malloc(nbyte);
    // example data for writing
    //std::cout<<"is there ?"<<std::endl;
    for (int i = 0; i < nbyte; i++) {
        buf[i] = '2';
    }
    if (buf == nullptr) {
        std::cerr << "malloc buf error!" << std::endl;
    }
    int te_num[100];
    int te_count = 0;
    for(int i = 0; i < 100; i++)
    {
        te_num[i] = 0;
    }
    //std::time_t start, end;
    long long rcount = 0;
    long long wcount = 0;
    double rtime = 0;
    double wtime = 0;

    struct timeval t1,t2;


    int tenement_id = 0;
    int tenement_count = 0;
    int is_bad = 0;

    while(get_instruction(trace, &ins, &num, &nblock)!= -1)
    {   
        int zone_id = 0;
        if(tenement_count == 0 && ins == 'w')//第一个租户
        {
            tenement_id = num;//第一次写  记录tenement_id
            std::list<zone_table> *list_head = new std::list<zone_table>();
            z_table[tenement_id] = list_head;
            tenement_count++;
        }
        //换租户
        if(num != tenement_id && ins == 'w')
        {
            tenement_id = num;
            std::list<zone_table> *new_list_head = new std::list<zone_table>();
            z_table[tenement_id] = new_list_head;
            te_count++;
        }   

        if(ins == 'w')
        {
            gettimeofday(&t1,NULL);
            for(int i = 0; i < nblock; i++){
                int offset = allo_block;

                zone_id = thread_id;

                allo_block++;
                if(allo_block >= max_block)
                break;


                if(offset == -1)
                {
                    is_bad = 1;
                    break;
                }

                zone_table new_block = {zone_id, offset};
                std::list<zone_table> *list = z_table[tenement_id];
                list->push_back(new_block);
                
               // debug;
                do_zone_write(dev_fd, zones + zone_id, buf, nbyte);
                wcount++;
            }
            gettimeofday(&t2,NULL);
            wtime += ((t2.tv_sec - t1.tv_sec) + (double)(t2.tv_usec - t1.tv_usec)/1000000.0);
            if(is_bad)
                break;
            //std::cout<<"in the write:"<<conf->z_table[tenement_id]->size()<<std::endl;
            ins ='a';
            num = 0;
            nblock = 0;
        }
        if(ins == 'r')
        {
            std::list<zone_table> *list = z_table[num];
            //std::cout<<"list size:"<<list->size()<<std::endl;
            gettimeofday(&t1,NULL);
            for(auto z : *list)
            {
                
                int zone_id = z.zone_id_;
                int nblock = z.off_block;
                //std::cout<<zone_id << " " << nblock<<std::endl;
                Byte *r_buf = (Byte*) malloc(nbyte);
                do_zone_read(dev_fd, zones+zone_id, r_buf, nbyte, nblock*size_block, false);
                rcount++;
                free(r_buf);
            };
            gettimeofday(&t2,NULL);
            rtime +=((t2.tv_sec - t1.tv_sec) + (double)(t2.tv_usec - t1.tv_usec)/1000000.0);
            ins ='a';
            num = 0;
            nblock = 0;
        }
    }

        re[count].w_count = wcount;
        re[count].w_time = wtime ;
        re[count].r_count = rcount;
        re[count].r_time = rtime ;
        //std::cout<<re[count].w_count<<std::endl;
}


void start_to_threads(int thread_count, re_value *re, zbd_zone * zones, int dev_fd)
{

    int i = 0;
    std::vector<std::thread> vth;
    for (i = 0; i < thread_count; i++)
    {
        //
        vth.push_back(std::thread(doo_io,i, re, zones, dev_fd));

    }

        for (auto vit = vth.begin(); vit!=vth.end(); vit++)
    {
        //
        if(vit->joinable())
            vit->join();
    }
}




int main(int argc, char *argv[]){

   //znsconf *conf = new znsconf();
    re_value *re = new re_value[num_thread];
    zbd_zone *zones;
    struct zbd_info info;
    int dev_fd;
    //debug;
    //init_conf(conf);
    //debug;

    get_options(argc, argv, &dev_fd, &info);
 
    //zbd_open_zones(dev_fd,0,671088640);
    unsigned int nr_zones;
    int ret = zbd_list_zones(dev_fd, 0, 0, zbd_report_option::ZBD_RO_ALL, &zones, &nr_zones);
    //std::cout<<dev_fd<<" "<<ret<<" "<<nr_zones<<" "<<zones<<std::endl;
    for(int i = 0; i < MAXZONE; ++i)
    {
        do_zone_ctl_operation(dev_fd,zones+i,ZBD_OP_OPEN);
        do_zone_ctl_operation(dev_fd, zones + i, ZBD_OP_RESET);
        update_zone_info(dev_fd, zones + i);
    }

    start_to_threads(num_thread, re,zones, dev_fd);

    long long total_write = 0, total_read = 0;
    double total_wtime = 0, total_rtime = 0;

    for(int i = 0; i < num_thread; i++)
    {
        total_write += re[i].w_count;
        total_read += re[i].r_count;
        total_wtime += re[i].w_time;
        total_rtime += re[i].r_time;
    }


    for(int i = 0; i < num_thread; i++){
        std::cout<<"Thread "<<i<<" write counts "<<re[i].w_count <<" write time "<<re[i].w_time<<\
        "s average rate: "<<(re[i].w_count) / (1024*64*re[i].w_time)<<"Gb/s"<<" write latency(s):"<<re[i].w_time / re[i].w_count<<std::endl;

        std::cout<<"Thread "<<i<<" read counts "<<re[i].r_count  <<" read time "<<re[i].r_time<<\
        "s average rate: "<<(re[i].r_count) / (1024*64*re[i].r_time)<<"Gb/s"<<" read latency(s):"<<re[i].r_time / re[i].r_count<<std::endl;

    }

    std::cout<<"total write counts "<<total_write <<" write time "<<total_wtime<<\
    "s average rate: "<<(total_write) / (1024*64*total_wtime)<<"Gb/s"<<" write latency(s):"<<total_wtime / total_write<<std::endl;
    std::cout<<"total read counts "<<total_read <<" write time "<<total_rtime<<\
    "s average rate: "<<(total_read) / (1024*64*total_rtime)<<"Gb/s"<<" write latency(s):"<<total_rtime / total_read<<std::endl;
    return 0;
}
