#!/bin/bash

echo "updating wheels"

for f in ./dist/*.whl
do
  echo "Processing $f file..."
  auditwheel repair -L "/core/libs" $f
done