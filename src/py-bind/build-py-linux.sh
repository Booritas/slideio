#!/bin/bash
set -e

rm -rf ./dist
rm -rf ../../build

echo "Build python wheels"

for dir in /opt/python/cp*
do
  export py="$dir/bin/python"
  echo "Processing python $py ..."
  rm -rf ./build
  rm -rf ../../build_py
  $py setup.py sdist bdist_wheel
done

echo "updating wheels"

for f in ./dist/*.whl
do
  echo "Processing $f file..."
  auditwheel repair $f
done