## Example usage with Rocket-Chip and OpenSBI

Start with [Rocket-Chip](https://github.com/chipsalliance/rocket-chip) and [OpenSBI](https://github.com/riscv-software-src/opensbi).

1. Clone all the repos.

```bash
git clone git@github.com:chipsalliance/rocket-chip.git
git clone git@github.com:cyyself/soc-simulator.git
git clone git@github.com:riscv-software-src/opensbi.git
```

2. Apply patch to rocket-chip

```patch
diff --git a/bootrom/bootrom.S b/bootrom/bootrom.S
index 4d8239367..3377426c3 100644
--- a/bootrom/bootrom.S
+++ b/bootrom/bootrom.S
@@ -13,9 +13,11 @@ _start:
 .globl _hang
 _hang:
   csrwi 0x7c1, 0 // disable chicken bits
+  li s0, DRAM_BASE
   csrr a0, mhartid
   la a1, _dtb
   csrwi mie, 0
+  jr s0
 1:
   wfi
   j 1b
diff --git a/src/main/scala/rocket/RocketCore.scala b/src/main/scala/rocket/RocketCore.scala
index 578ae4345..ace1b8498 100644
--- a/src/main/scala/rocket/RocketCore.scala
+++ b/src/main/scala/rocket/RocketCore.scala
@@ -974,6 +974,7 @@ class Rocket(tile: RocketTile)(implicit p: Parameters) extends CoreModule()(p)
   }
   else {
     when (csr.io.trace(0).valid) {
+      /*
       printf("C%d: %d [%d] pc=[%x] W[r%d=%x][%d] R[r%d=%x] R[r%d=%x] inst=[%x] DASM(%x)\n",
          io.hartid, coreMonitorBundle.timer, coreMonitorBundle.valid,
          coreMonitorBundle.pc,
@@ -985,6 +986,7 @@ class Rocket(tile: RocketTile)(implicit p: Parameters) extends CoreModule()(p)
          Mux(wb_ctrl.rxs2 || wb_ctrl.rfs2, coreMonitorBundle.rd1src, 0.U),
          Mux(wb_ctrl.rxs2 || wb_ctrl.rfs2, coreMonitorBundle.rd1val, 0.U),
          coreMonitorBundle.inst, coreMonitorBundle.inst)
+       */
     }
   }
 
diff --git a/src/main/scala/subsystem/Configs.scala b/src/main/scala/subsystem/Configs.scala
index 03de3af1a..ecf288f6c 100644
--- a/src/main/scala/subsystem/Configs.scala
+++ b/src/main/scala/subsystem/Configs.scala
@@ -395,7 +395,7 @@ class WithTimebase(hertz: BigInt) extends Config((site, here, up) => {
 class WithDefaultMemPort extends Config((site, here, up) => {
   case ExtMem => Some(MemoryPortParams(MasterPortParams(
                       base = x"8000_0000",
-                      size = x"1000_0000",
+                      size = x"8000_0000",
                       beatBytes = site(MemoryBusKey).beatBytes,
                       idBits = 4), 1))
 })
diff --git a/src/main/scala/system/Configs.scala b/src/main/scala/system/Configs.scala
index fdc8a87b4..9786853c4 100644
--- a/src/main/scala/system/Configs.scala
+++ b/src/main/scala/system/Configs.scala
@@ -83,4 +83,4 @@ class MMIOPortOnlyConfig extends Config(
 )
 
 class BaseFPGAConfig extends Config(new BaseConfig ++ new WithCoherentBusTopology)
-class DefaultFPGAConfig extends Config(new WithNSmallCores(1) ++ new BaseFPGAConfig)
+class DefaultFPGAConfig extends Config(new WithNBigCores(1) ++ new BaseFPGAConfig)
```

We have changed the following things:

    1. Set bootrom to jump to DRAM_BASE address.
    2. Commented trace output, so we can see the output from Serial.
    3. Set proper memory size.
    4. Change Small Core to Big Core, since the Small Core has no S-Mode support to get OpenSBI running.


3. Build Rocket-Chip

```bash
pushd rocket-chip
pushd bootrom
make
popd
pushd vsim
make verilog CONFIG=freechips.rocketchip.system.DefaultFPGAConfig
popd
popd
```

4. Build Device Tree and OpenSBI

```bash
pushd opensbi
cp ../rocket-chip/vsim/generated-src/freechips.rocketchip.system.DefaultFPGAConfig.dts rocket.dts
```

Apply this patch to rocket.dts.

```patch
7a8,28
> 
>     chosen {
>         bootargs = "earlycon=sbi console=ttyUL0 rdinit=/sbin/init";
>         stdout-path = "serial0";
>     };
> 
>       aliases {
>               serial0 = &uart0;
>       };
> 
>     uart0: uartlite_0@60100000 {
>         compatible = "xlnx,axi-uartlite-1.02.a", "xlnx,xps-uartlite-1.00.a";
>         reg = <0x60100000 0x1000>;
>         interrupt-parent = <&L5>;
>         interrupts = <1>;
>         clock = <&L0>;
>         current-speed = <115200>;
>         xlnx,data-bits = <8>;
>         xlnx,use-parity = <0>;
>     };
> 
```

```bash
dtc rocket.dts > rocket.dtb
make CROSS_COMPILE=riscv64-unknown-linux-gnu- PLATFORM=generic FW_FDT_PATH=rocket.dtb -j 16
popd
```

5. Build SoC-Simulator and run

```bash
cd soc-simulator
make
./obj_dir/VExampleRocketSystem
```

Note: It takes about 30s when simulating on AMD Ryzen 5800X CPU to see the first serial output and you will finally see the "Test payload running". If you didn't see any output, you can uncomment trace output in step 2 to see what happens.

## Advanved Usage

Go to see [src/sim_rocket.cpp](../src/sim_rocket.cpp) and [Makefile](../Makefile), you will learn how to build verilog to Verilator and connect the AXI wire and set up soc in C++ software with defined address space.