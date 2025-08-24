#!/bin/bash

javac LockBenchmark.java

java LockBenchmark 128 1 >>java_reentrant.csv
java LockBenchmark 128 2 >>java_synchronized.csv

