## Verilator仿真相比Vivado仿真的优劣对比

优点：
1. 仿真速度非常快，方便快速调试。
2. 不上板可不必安装Vivado，配合gtkwave仅需要下载不到200M的软件，非常轻量。

缺点：
1. 不是四态仿真器，不考虑X和Z，无法发现一些reg未初始化等错误。
2. 不支持调用Xilinx IP，同学们若在乘除法器等组件使用Xilinx IP会导致无法接入Verilator。

## 如何用于仿真调试功能测试？

1. 将CPU代码放置到相对于本文件夹的`../../mycpu`文件夹中。
2. 在本文件夹（即`func_sram`）下，使用make命令完成编译
3. 使用`./obj_dir/Vmycpu_top`，运行处理器，自动开始仿真测试
4. 如果看到`Number 89 Functional Test Point PASS!`，表示89个测试点均通过。
5. 如果失败，可以使用`gtkwave`打开`trace.vcd`文件，对波形图进行调试。

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

2. 还是无法解决时，建议向助教求助。

## Verilator可以仿真，但Vivado无法仿真怎么办？

1. 检查是否有未初始化的reg，未连接的wire，这些在Verilator中会被认为是0，但Vivado四态仿真器会出现X和Z。
2. 还是无法解决时，建议向助教求助。