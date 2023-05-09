## wlmalloc一个简单的内存分配器以及单线程垃圾回收器wlgc

拉取代码：`git clone git@github.com:zinkt/wlmalloc.git`  
在wlmalloc目录下，使用编写好的Makefile进行编译：`make`   
在src目录下将会有静态链接库libwlmalloc.a和动态链接库libwlmalloc.so，可供进一步使用。

### 使用内存分配器

wlmalloc对外接口：   
```c
void *wl_malloc(size_t sz); // 内存分配接口
void wl_free(void *ptr); // 内存释放接口
// 三模冗余 分配，释放，读（或者说check），写
void *tri_mod_alloc(size_t size);
void *tri_mod_free(void *ptr);
void *tri_mod_read(void *ptr);
void *tri_mod_write(void *ptr, void *source, size_t size);
```
在您的项目(如名为test.c)中引入头文件：#include “wlmalloc.h”    
可以使用静态库方式调用：   
-L指定静态库文件库搜索路径，-l指定库名    
gcc test.c -L. -lwlmalloc -o test      
./test      
也可以使用动态库方式隐式调用：   
LD_PRELOAD=./libmalloc.so ./test    
或通过一下方式使得系统找到动态库文件：  

1. 将libwlmalloc.so库文件复制到/lib或/usr/lib目录下，那么ld默认能够找到，无需其他操作。
2. 如果libwlmalloc.so在其他目录，需要将其添加到/etc/ld.so.cache文件中，步骤如下：
3. 编辑/etc/ld.so.conf文件，加入库文件所在目录的路径
4. 运行ldconfig，该命令会重建/etc/ld.so.cache文件

### 使用垃圾回收器

wlgc对外接口：    
```c
// 使用前需调用gc_init()设置调用函数的首地址为gc的栈底
// 对于想要跟踪的内存，调用gc_malloc进行自动管理
void gc_init();// 由使用者调用，将会设置调用者的首地址为gc的栈底
void gc_collect();// 手动触发垃圾回收接口
void *gc_malloc(size_t size); // 需要进行跟踪并回收的内存分配接口
void gc_free(void *ptr); // 手动释放某个已被跟踪的内存
void *gc_calloc(size_t nmemb, size_t size);
void *gc_realloc(void *ptr, size_t size);
size_t gc_collected();// 用于统计回收内存块数目
```
类似的，在你的项目(如名为test.c)中引入头文件：#include “wlgc.h”
由于代码量不多，建议直接将代码复制入项目中引用。

### 分配器wlmalloc测试
功能可用性（正确性）：
由于简单地写一些调用代码，对于可用性的验证不够全面。因此本文采用直接劫持系统libc中的malloc/calloc/realloc/free函数，调用一些Linux常用指令来进行验证。    
在Linux系统中，存在一个名为LD_PRELOAD的环境变量。当LD_PRELOAD指定了某个文件时，动态链接器会在按照一定规则搜索共享库之前，先加载该文件。因此，使用LD_PRELOAD可以优先于LD_LIBRARY_PATH指定的目录加载共享库。无论程序是否依赖于它们，LD_PRELOAD里面指定的共享库或目标文件都会被装载。
因此，本测试通过设置LD_PRELOAD环境变量，从而优先加载limalloc.so，劫持系统malloc函数，通过在Linux系统中使用实际的常用系统指令，来测试wlmalloc的功能可用性。

执行ls指令；`LD_PRELOAD=./libmalloc.so ls`
 
执行cat指令：`LD_PRELOAD=./libmalloc.so cat tt.c`
 
执行wc指令：`LD_PRELOAD=./libmalloc.so wc wlmalloc.c`

本文对 wlmalloc的benchmark参考的是github上为测试mimalloc所开源的基准测试套件：mimalloc-bench[https://github.com/daanx/mimalloc-bench]

由于该基准测试完整项目较大，运行脚本构建后更大，包含了诸如tcmalloc，jemalloc等分配器的源码以及编译完成的库文件，测试程序集等，且项目中的README文件展示了较为详尽的介绍和使用方法。因此本文的代码库中只包含修改过的bench.sh这个执行测试的脚本。使用方式也极为简单，只需将编译完成的libwlmalloc.so放入mimalloc-bench/extern/wl目录下，替换bench.sh文件，运行bench.sh即可。
 
### 回收器wlgc测试

参考一个gc-benchmark的实现，分为三部分：

1. 构建一个临时大二叉树对内存进行“拉伸”。即创建完成后，将根结点指针赋值为0（NULL），整棵树则成为垃圾。时机合适时，会触发gc_collect()将树所占据的空间收集掉。
2. 构建一个长时间存活的大二叉树、一个赋值的大数组，占用一些空间。
3. 由最小深度为kMinTreeDepth，到最大深度kMaxTreeDepth，步长为2，分别从高到低和从低到高，构建多棵大二叉树，数量取决于当前树的深度，保证每次分配节点的数量相同。

执行测试：  
在本测试用例中，kMinTreeDepth=4， kMaxTreeDepth=14  
若此前未执行make，则单独执行make benchmark，运行测试程序：./benchmark/gcbench.out   
 
## 结构

- src：源代码及功能测试
- benchmark：性能测试
- doc：文档(未更新)