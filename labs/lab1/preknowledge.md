本节内容需要深入整个计算机系统贯通理解。因此我们需要首先对cpu/内存 外的世界有更多的了解。
# 硬件
## 外设
### 内存映射 IO
现代计算机通过读写特定的物理地址来完成对外设的读写。UART 的发送数据寄存器可能被映射到物理地址 0x1000_0000。软件想发送一个字符 A，只需要执行一条存储指令：sb a0, 0x1000_0000（将寄存器 a0 中的值存到该地址）。硬件会识别出这个地址属于 UART 而非内存，并将数据送入串口。

### 中断控制器
为cpu提供中断服务的硬件设备，譬如plic。中断的工作流程如下所示
- 触发：外设完成工作后拉高中断信号线
- 仲裁：中断信号发送到中断处理器，中断处理器根据优先级仲裁出优先处理的中断对象
- 通知：中断处理器通知cpu处理中断
- 响应：cpu保存上下文，跳转到中断服务程序（ISR）执行中断响应
- 处理：ISR读取外设状态，处理数据
- 清除：ISR通知外设中断已处理，然后恢复现场，继续之前的工作





# makefile
基本格式
```makefile
target : prerequisites
    command1
    command2
    ...
```
command 可以是sh脚本

支持通配符`*,?,~`

makefile 中的变量就是c中的macro

## 文件搜寻

```make
VPATH = src:../headers
```

或者设定vpath关键字

```makefile
vpath <pattern> <directories> 
vpath <pattern> # 设定为空，也即清除
vpath # 全部清除
```

pattern使用 %字符，表示匹配0或若干字符（匹配多个字符）

directory可以是 foo:bar用分号隔开。

## 伪目标
通过设定伪目标执行特定的任务，这些任务一般都不用生成特定的文件。

例子
> ```make
> all : prog1 prog2 prog3
> .PHONY : all
> 
> prog1 : prog1.o utils.o
>     cc -o prog1 prog1.o utils.o
> 
> prog2 : prog2.o
>     cc -o prog2 prog2.o
> 
> prog3 : prog3.o sort.o utils.o
>     cc -o prog3 prog3.o sort.o utils.o
> ```
> Makefile中的第一个目标会被作为其默认目标。我们声明了一个“all”的伪目标，其依赖于其它三个目标。由于默认目标的特性是，总是被执行的，但由于“all”又是一个伪目标，伪目标只是一个标签不会生成文件，所以不会有“all”文件产生。于是，其它三个目标的规则总是会被决议。也就达到了我们一口气生成多个目标的目的。 .PHONY : all 声明了“all”这个目标为“伪目标”。（注：这里的显式“.PHONY : all” 不写的话一般情况也可以正确的执行，这样make可通过隐式规则推导出， “all” 是一个伪目标，执行make不会生成“all”文件，而执行后面的多个目标。）

make的时候会检测目标是否最新，如果当前文件下有了最新的clean，那么就不会运行clean操作，因此需要伪目标，意味着这是一个操作，而不是生成特定的目标。


## 静态模式
语法
```make
<targets ...> : <target-pattern> : <prereq-patterns ...>
    <commands>
    ...
```

比如这份写好了的makefile
```makefile
SRC=printk.c
OBJ=$(SRC:.c:.o)

%.o:%.c
	$(GCC) $(CFLAG) -c $< -o $@

all: $(OBJ)

clean:
	rm -f $(OBJ)

.PHONY: all clean
```

其中制定了我们要的OBJ，然后制定了这些OBJ应该使用什么依赖和方法来编译。