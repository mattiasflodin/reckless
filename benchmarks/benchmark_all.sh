#!/bin/bash
rm log.txt
./asynclog_periodic_calls > /dev/null
rm log.txt
./asynclog_periodic_calls > asynclog_periodic_calls.txt
rm log.txt
./spdlog_periodic_calls > /dev/null
rm log.txt
./spdlog_periodic_calls > spdlog_periodic_calls.txt
