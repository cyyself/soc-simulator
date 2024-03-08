TOP_NAME := T1Subsystem
INC_FILE := ../t1/result/*.sv
INC_DIR	 := ../t1/result/
.PHONY: obj_dir/V$(TOP_NAME)
obj_dir/V$(TOP_NAME): src/* $(INC_FILE)
	verilator --trace --cc -Wno-fatal --exe -LDFLAGS "-lpthread" --build src/sim_t1.cpp $(INC_FILE) -I$(INC_DIR) --top $(TOP_NAME) --CFLAGS "-Os" -j `nproc`