#!/bin/bash
g++ -std=c++17 ex1.cpp -o ex1 \
    -I/Libraries/Skywing/skywing \
    -I/Libraries/Skywing/subprojects/spdlog/include \
    -I/Libraries/Skywing/subprojects/gsl/include \
    -I/Libraries/Skywing/build/generated_files/generated \
    -I/Libraries/Skywing/build/generated_files/generated `pkg-config capnp --cflags --libs` \
    -L/Libraries/Skywing/build/skywing/skywing_core \
    -lskywing_core

    