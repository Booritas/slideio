for i in $(find ~/.conan/data -name \*.a); do # Not recommended, will break on whitespace
    if nm -u "$i" | grep 'lerc_decode'
    then
        echo "$i"
    fi
done