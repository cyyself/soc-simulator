TOP_NAME := ExampleRocketSystem
INC_FILE := ../rocket-chip/out/emulator/freechips.rocketchip.system.TestHarness/freechips.rocketchip.system.DefaultFPGAConfig/mfccompiler/compile.dest/*.v ../rocket-chip/out/emulator/freechips.rocketchip.system.TestHarness/freechips.rocketchip.system.DefaultFPGAConfig/mfccompiler/compile.dest/*.sv
INC_DIR	 := ../rocket-chip/out/emulator/freechips.rocketchip.system.TestHarness/freechips.rocketchip.system.DefaultFPGAConfig/mfccompiler/compile.dest/

.PHONY: sim_rocket sim_qemu

sim_rocket: obj_dir/V$(TOP_NAME)

obj_dir/V$(TOP_NAME): src/* $(INC_FILE)
	verilator --cc -Wno-fatal --exe -LDFLAGS "-lpthread" --build src/sim_rocket.cpp $(INC_FILE) -I$(INC_DIR) --top $(TOP_NAME) --CFLAGS "-O3 -std=c++20" -j `nproc`

sim_qemu: obj_dir_qemu/V$(TOP_NAME)

obj_dir_qemu/V$(TOP_NAME): src/* $(INC_FILE)
	verilator --cc -Wno-fatal --exe -LDFLAGS "-lpthread" --build src/sim_qemu.cpp $(INC_FILE) -I$(INC_DIR) --top $(TOP_NAME) --CFLAGS "-O3 -std=c++20 -I../../protocol" -Mdir obj_dir_qemu -j `nproc`
