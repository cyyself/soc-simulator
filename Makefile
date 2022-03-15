TOP_NAME := ChipTop
INC_FILE := ../chipyard/sims/verilator/generated-src/chipyard.TestHarness.MyBoomConfig/*.v
INC_DIR	 := ../chipyard/sims/verilator/generated-src/chipyard.TestHarness.MyBoomConfig
.PHONY: obj_dir/V$(TOP_NAME)
obj_dir/V$(TOP_NAME): src/* $(INC_FILE)
	verilator --cc -Wno-fatal --trace --trace-structs --exe -LDFLAGS "-lpthread" --build src/sim_soc.cpp $(INC_FILE) -I$(INC_DIR) --top $(TOP_NAME) --CFLAGS "-Ofast"