#!/bin/bash

for t in $(find . -name "test_*.py" | sort)
do
    python "$t" || exit 1
done
