# Document


# How to compile

```
g++ --version
g++ (Ubuntu 11.3.0-1ubuntu1~22.04) 11.3.0


g++ -O2 bench.cpp -o ben -l:libzbd.a -pthread 
```

# How to run

##  Parameter
```
-b num_block_perzone  indicate the number of blocks in one zone
-f filename           indicate the position of zns ssd  
-p num_thread         indicate the the number of parallel threads

eg.
sudo  ./ben -b 1000 -f /dev/nullb0 -p 8
```
# Workload 
50% write and 50% read to emulate ML job


# Outcome
```
like this


Thread 0 write counts 912 write time 0.007909s average rate: 1.75952Gb/s write latency(s):8.67215e-06
Thread 0 read counts 816 read time 0.002476s average rate: 5.02874Gb/s read latency(s):3.03431e-06
Thread 1 write counts 912 write time 0.007007s average rate: 1.98602Gb/s write latency(s):7.68311e-06
Thread 1 read counts 816 read time 0.002636s average rate: 4.72351Gb/s read latency(s):3.23039e-06
Thread 2 write counts 912 write time 0.00643s average rate: 2.16423Gb/s write latency(s):7.05044e-06
Thread 2 read counts 816 read time 0.002365s average rate: 5.26477Gb/s read latency(s):2.89828e-06
Thread 3 write counts 912 write time 0.007641s average rate: 1.82123Gb/s write latency(s):8.37829e-06
Thread 3 read counts 816 read time 0.002752s average rate: 4.52441Gb/s read latency(s):3.37255e-06
Thread 4 write counts 912 write time 0.006365s average rate: 2.18633Gb/s write latency(s):6.97917e-06
Thread 4 read counts 816 read time 0.00226s average rate: 5.50937Gb/s read latency(s):2.76961e-06
Thread 5 write counts 912 write time 0.007212s average rate: 1.92956Gb/s write latency(s):7.90789e-06
Thread 5 read counts 816 read time 0.002597s average rate: 4.79444Gb/s read latency(s):3.1826e-06
Thread 6 write counts 912 write time 0.006265s average rate: 2.22123Gb/s write latency(s):6.86952e-06
Thread 6 read counts 816 read time 0.002406s average rate: 5.17505Gb/s read latency(s):2.94853e-06
Thread 7 write counts 912 write time 0.007046s average rate: 1.97502Gb/s write latency(s):7.72588e-06
Thread 7 read counts 816 read time 0.002738s average rate: 4.54754Gb/s read latency(s):3.35539e-06
total write counts 7296 write time 0.055875s average rate: 1.99245Gb/s write latency(s):7.65831e-06
total read counts 6528 write time 0.02023s average rate: 4.92384Gb/s write latency(s):3.09896e-06

```

# note
I assume the size of a block is 4k bits and write 8 blocks each time    only 10 zones open without any policy <br>