TOP_NAME := mycpu_top
SRC_DIR  := ../NSCSCC-2022
INC_FILE := $(shell find $(SRC_DIR) -name '*.v') $(shell find $(SRC_DIR) -name '*.sv') $(shell find $(SRC_DIR) -name '*.vh')
.PHONY: obj_dir/V$(TOP_NAME)
obj_dir/V$(TOP_NAME): src/* $(INC_FILE)
	verilator --cc -Wno-fatal --exe --trace --trace-structs -LDFLAGS "-lpthread" --build src/sim_mycpu.cpp $(INC_FILE) -I$(SRC_DIR) --top $(TOP_NAME) --CFLAGS "-Os" -j 24