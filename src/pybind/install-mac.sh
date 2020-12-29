set -e
export CI_PIPELINE_IID=0
pythons=( "3.9" "3.8" "3.7" "3.6" "3.5" )
for version in "${pythons[@]}"
do
   rm -rf ./build
   rm -rf ../../build_py
   pversion="python/${version}@slideio/stable"
   echo $pversion
   conan install $pversion -g json -if conan
   binpath=`python pyinfo.py`
   pypath="${binpath}/bin/python"
   $pypath setup.py sdist bdist_wheel
done
