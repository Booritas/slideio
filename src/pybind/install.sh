/opt/python/cp36-cp36m/bin/python setup.py sdist bdist_wheel
/opt/python/cp37-cp37m/bin/python setup.py sdist bdist_wheel
/opt/python/cp38-cp38/bin/python setup.py sdist bdist_wheel
/opt/python/cp39-cp39/bin/python setup.py sdist bdist_wheel
/opt/python/cp310-cp310/bin/python setup.py sdist bdist_wheel

for f in ./dist/*.whl
do
  echo "Processing $f file..."
  auditwheel repair $f
done
