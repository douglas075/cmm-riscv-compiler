#!/usr/bin/env bash

dir="./tests/"
echo "----- semantic fail testcases -----"
for i in {0..17}; do
    file="fail${i}.c"
    path="${dir}${file}"
    echo "Running ${path}"
    # Example command:
    ./compiler < ${path}
	echo ""
done


echo "----- syntax fail testcases -----"
for i in {1..3}; do
    file="pfail${i}.c"
    path="${dir}${file}"
    echo "Running ${path}"
    # Example command:
    ./compiler < ${path}
	echo ""
done

echo "----- valid testcases -----"
for i in {1..9}; do
    file="test${i}.c"
    path="${dir}${file}"
    echo "Running ${path}"
    # Example command:
    ./compiler < ${path}
	echo ""
done

echo "----- sample testcases -----"
for i in {1..3}; do
    file="sample${i}.c"
    path="${dir}${file}"
    echo "Running ${path}"
    # Example command:
    ./compiler < ${path}
	echo ""
done
