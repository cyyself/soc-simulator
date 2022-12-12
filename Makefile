TOP_NAME := mycpu_top
SRC_DIR  := ../../mycpu
INC_FILE := $(shell find $(SRC_DIR) -name '*.svh') $(shell find $(SRC_DIR) -name '*.vh') $(shell find $(SRC_DIR) -name '*.v') $(shell find $(SRC_DIR) -name '*.sv')
INC_DIR	 := $(addprefix -I, $(shell find $(SRC_DIR) -type d))
.PHONY: clean
obj_dir/V$(TOP_NAME): src/* $(INC_FILE)
	verilator --cc -Wno-fatal --exe --trace --trace-structs --build src/sim_nscscc.cpp $(INC_FILE) $(INC_DIR) --top $(TOP_NAME) -j `nproc`

clean:
	rm -rf obj_dir