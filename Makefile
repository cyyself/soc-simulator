TOP_NAME := ExampleRocketSystem
INC_FILE := ../rocket-chip/vsim/generated-src/*.v 
INC_DIR	 := ../rocket-chip/vsim/generated-src/freechips.rocketchip.system.DefaultFPGAConfig
.PHONY: obj_dir/V$(TOP_NAME)
obj_dir/V$(TOP_NAME): src/* $(INC_FILE)
	verilator --cc -Wno-fatal --exe -LDFLAGS "-lpthread" --build src/sim_rocket.cpp $(INC_FILE) -I$(INC_DIR) --top $(TOP_NAME) --CFLAGS "-Os -std=c++20" -j `nproc`