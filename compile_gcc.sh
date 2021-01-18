#! /bin/sh

g++ audio_resampler.cpp -g -lavformat -lavcodec -lavutil -lswresample -ldl -lpthread -lz -lbz2 -std=c++11  -L/usr/local/lib -lx264 -lfdk-aac
