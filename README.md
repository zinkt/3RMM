## wlmalloc一个简单的内存分配器

1. 直接使用源代码，与项目一同编译
2. 动态链接库的方式
   1. 进入src/目录，执行`make so`，即可得到libwlmalloc.so动态链接库文件
   2. 在项目中引入头文件`#include "wlmalloc.h"`
   3. gcc编译时链接库文件`-lwlmalloc`

## 历程

1. 学习了CSAPP中关于内存分配器那一节的知识，并完成了对应的实验。见lab_impl
2. 有了一定知识后，以自己的想法实现一个内存分配器。见trying_impl
3. 参考多种商用内存分配器，tcmalloc, mimalloc。但其实现细节过于复杂，我们只能把握其思想，为了理解并实现，参考了其他一些个人实现。

## 结构

- src：源代码及功能测试
- benchmark：性能测试
- lab_impl：csapp中的内存分配器，前期学习
- doc：文档