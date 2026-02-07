#!/usr/bin/env bash
set -e
ulimit -n 4096
python3 sample/08_signal_test/test_signal.py
