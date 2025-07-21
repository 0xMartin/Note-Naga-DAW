#!/usr/bin/env bash

cppcheck \
  --enable=all \
  --inconclusive \
  --std=c++20 \
  --language=c++ \
  --template=gcc \
  --quiet \
  -i build \
  -i external \
  src note_naga_engine include