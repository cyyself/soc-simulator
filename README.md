## Verilator仿真相比Vivado仿真的优劣对比

优点：
1. 仿真速度非常快，方便快速调试。
2. 不上板可不必安装Vivado，配合gtkwave仅需要下载不到200M的软件，非常轻量。

缺点：
1. 不是四态仿真器，不考虑X和Z，无法发现一些reg未初始化等错误。
2. 不支持调用Xilinx IP，同学们若在乘除法器等组件使用Xilinx IP会导致无法接入Verilator。

## 如何用于仿真调试功能测试？

1. 将CPU代码放置到`../../mycpu`文件夹中。
2. 在本文件夹（即`func_sram`）下，使用make命令完成编译
3. 使用`./obj_dir/Vmycpu_top`，运行处理器，自动开始仿真测试
4. 如果看到`Number 89 Functional Test Point PASS!`，表示89个测试点均通过。
5. 如果失败，可以使用`gtkwave`打开`trace.vcd`文件，对波形图进行调试。