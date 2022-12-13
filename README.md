## Verilator仿真相比Vivado仿真的优劣对比

优点：
1. 仿真速度非常快，方便快速调试。
2. 不上板可不必安装Vivado，配合gtkwave仅需要下载不到200M的软件，非常轻量。

缺点：
1. 不是四态仿真器，不考虑X和Z，无法发现一些reg未初始化等错误。
2. 不支持调用Xilinx IP，同学们若在乘除法器等组件使用Xilinx IP会导致无法接入Verilator。

## 如何编译？

1. 将CPU代码放置到相对于本文件夹的`../../mycpu`文件夹中。
2. 在本文件夹（即`verilator/axi`）下，使用`make`命令完成编译
3. 编译结果位于`obj_dir/Vmycpu_top`
4. 每次修改CPU代码后，需要重新`make`，如果引入了一些时间早于编译产物的代码，需要先`make clean`再`make`

## 参数说明

运行模式（3选1，同时使用时取最后一个）：
- `-func` 运行功能测试（带trace比对）
- `-perf` 运行性能测试
- `-perfdiff` 运行性能测试的差分测试模式（可及时定位性能测试中的错误），该模式下，性能测试中的程序只会被执行一次以方便debug。

其他参数：
- `-trace [trace时间]` 输出波形图，对于功能测试，波形图会输出到`trace.vcd`，对于性能测试，波形图会输出到`trace-perf-[性能测试点编号].vcd`。限定trace时间是为了防止输出过多波形图导致爆硬盘，如果你相信你的处理器不会死锁，可以直接使用`-trace 1000000000`。

性能测试（含性能测试差分测试）可用参数：
- `-uart` 打印confreg串口，用于性能测试中输出测试程序的标准输出。
- `-prog [1-10]` 设置性能测试的测试点编号，若不指定则依次运行10个测试。
- `-axifast` 关闭AXI延迟，运行更快，但得分将与上板结果可能存在显著差异。
- `-perfonce` 性能测试每个测试点只运行一次（默认运行10次），仅用于Debug，跑分结果不作为成绩依据。

性能差分测试可用参数
- `-diffuart` 对比串口输出结果，用于调试Uncached访存。

举例：
1. 直接运行功能测试，并输出波形图最长1e9的时间。 `./obj_dir/Vmycpu_top -func -trace 1000000000`
2. 运行性能测试的第3个程序并打印串口以及输出波形图。 `./obj_dir/Vmycpu_top -perfdiff -prog 3 -trace 100000000 -uart`
3. 运行完整性能测试得到IPC分数。 `./obj_dir/Vmycpu_top -perf`

## Vivado可以仿真，但Verilator无法仿真怎么办：

1. 使用了Xilinx IP的情况，无解，请自行替换相应IP核，Block Memory可以参照[该代码](https://github.com/Maxpicca-Li/CDIM/blob/master/mycpu/4mem/dual_port_bram_bw8.sv)编写，与使用BRAM IP在硬件上完全相同，且省去了综合BRAM的时间。
2. 检查代码的wire、reg名称是否出现C++关键字，例如`int`,`break`,`type`等，若有需要修改。
3. 检查是否存在组合逻辑对同一个wire内部不同的位进行了相互依赖，若存在，可以自行拆解。
4. 检查是否存在使用for循环对数组赋值，Verilator不支持，例如以下代码：

    ```verilog
    for(t=0; t<CACHE_DEEPTH; t=t+1) begin   //刚开始将Cache置为无效
        cache_valid[t] <= 0;
    end
    ```

    可以修改为：

    ```verilog
    cache_valid <= '{default: '0};
    ```

5. 还是无法解决时，建议向助教求助。

## Verilator可以仿真，但Vivado无法仿真怎么办？

1. 检查是否有未初始化的reg，未连接的wire，这些在Verilator中会被认为是0，但Vivado四态仿真器会出现X和Z。
2. 还是无法解决时，建议向助教求助。

## 为什么组成原理实验中给的Cache Lab的处理器也无法运行部分测试点？

因为该处理器存在许多Bug，需要同学们自己发现，并非该框架问题。