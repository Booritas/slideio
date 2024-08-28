#!/bin/bash

build_wheel()
{
  echo "Processing python $py ..."
  rm -rf ./build
  rm -rf ../../build_py
  $py setup.py sdist bdist_wheel
}

set -e

rm -rf ./dist
rm -rf ../../build

echo "Build python wheels"

for dir in /opt/python/cp*
 do
  if [[ "$dir" != *"36"* ]]; then
      export py="$dir/bin/python"
      build_wheel
  fi
 done

#export py="/opt/python/cp38-cp38/bin/python"
#build_wheel

echo "updating wheels"

for f in ./dist/*.whl
do
  echo "Processing $f file..."
  auditwheel repair $f
done