# course 2
## outline
![outline-of-chapter-2](imgs/chapter2/outline.png)
## OS Services
目的是让 **用户** 和 **OS** 本身更好地使用计算机。

- for user
  - 各种 UI（GUI/CLI/touch screen）
  - 执行程序
  - IO 操作
  - 文件系统
  - 网络通信
  - 检测错误：环境错误、程序运行错误等等。
- for OS
  - 分配资源
  - 记录资源使用情况（accounting，记账）
  - protection/security。

操作系统提供了cli接口供用户使用，可以内置在kernel中，也可以作为system program。它能执行各种各样设计好的命令，这些命令可以是内置的（比如cmd），可以是许多的小程序的name（bash）。

## OS call
以上Service会实现成为system call。system call就是OS提供的PI，一般用c/c++写。直接的sc还是太抽象了，因为它还是接近底层。有很多基于sc的api，更易用，更可移植，比如win32api，posix api和java api。

每个sc都有一个id，由system call interface管理。同时sci负责代理用户程序调用sc，并返回运行结果，因此caller（user program）不知道执行过程，只需要调用sci的api，便可得到结果。许多runtime support library（rts）就是这样，在运行时提供对OS资源的访问，并且程序员无需直接操作内核。

### si parameters passing
sc 需要不少参数。比如要执行的是什么sc，相关的参数有多少/在哪里等等。

传参的三个方式
- registers（传寄存器）
- parameters stored in a block/table in memory，then use address of the block as a parameter.（传地址）
- 栈参数。
## types of si
- 进程控制相关：fork/wait/exit
- 文件相关：open/read/write/close
- 文件权限：chmod/umask/chown
- 设备相关：ioctl/read/write
- 获取信息：getpid
- 通信：pipe等

## system program
sp provide a convenient environment for program development and execution.
比如各种sh的命令，perf/gdb，编译器、链接器。这种层面的东西就可以称为sp。

系统程序侧重于系统管理和资源访问。
# appendix: 一个脑图
操作系统要实现的功能->system call一个底层实现->便于使用 system program。