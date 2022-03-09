TOP_NAME := ChipTop
INC_FILE := ../chipyard/sims/verilator/generated-src/chipyard.TestHarness.MyBoomConfig/*.v
INC_DIR	 := ../chipyard/sims/verilator/generated-src/chipyard.TestHarness.MyBoomConfig
.PHONY: obj_dir/V$(TOP_NAME)
obj_dir/V$(TOP_NAME): src/sim_soc.cpp
	verilator --cc -Wno-fatal --exe --build src/sim_soc.cpp $(INC_FILE) -I$(INC_DIR) --top $(TOP_NAME)