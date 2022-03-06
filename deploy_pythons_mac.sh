set -e
echo "Recipes repository location:${CONAN_CUSTOM_DIR}"
pythons=("3.9" "3.8" "3.7" "3.6" "3.5" )
for version in "${pythons[@]}"
do
   pversion="python-${version}"
   echo $pversion
   conan create ${CONAN_CUSTOM_DIR}/${pversion} slideio/stable
doness
