## mem hook 库

我们想要对线程的内存使用情况进行监控。目前我们使用的 hook 内存管理函数的方式。

我们 hook 了如下内存管理函数：
申请内存：【malloc、calloc、realloc、reallocarray、posix_memalign、aligned_alloc、memalign、valloc、pvalloc】
释放内存：【free】

### 编译生成库

```
mkdir build
cd build
cmake ..
make
```
即可生成 so 库