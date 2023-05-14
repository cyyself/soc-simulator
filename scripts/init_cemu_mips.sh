#!/bin/sh
(git clone https://github.com/cyyself/cemu.git || stat cemu) && cp cemu/src/core/mips/* src/ && cp cemu/src/memory/memory_bus.hpp src/