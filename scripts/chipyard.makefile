TOP_NAME := ChipTop
INC_FILE := ../chipyard/sims/verilator/generated-src/chipyard.harness.TestHarness.CYYSoCConfig/gen-collateral/*.sv \
			../chipyard/sims/verilator/generated-src/chipyard.harness.TestHarness.CYYSoCConfig/gen-collateral/*.mems.v
INC_DIR	 := ../chipyard/sims/verilator/generated-src/chipyard.harness.TestHarness.CYYSoCConfig/gen-collateral
.PHONY: obj_dir/V$(TOP_NAME)
obj_dir/V$(TOP_NAME): src/* $(INC_FILE)
	verilator --cc -Wno-fatal --exe --trace-fst --trace-structs -LDFLAGS "-lpthread" --build src/sim_chipyard.cpp $(INC_FILE) -I$(INC_DIR) --top $(TOP_NAME) --CFLAGS "-Os" -j `nproc`