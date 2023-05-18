for i in $(find ~/.conan/data/dcmtk/3.6.7/slideio/stable/package/e74e5b21398b0e3b4122709730d7c8b2958b9796/lib -name \*.a); do # Not recommended, will break on whitespace
    if nm -C "$i" | grep 'findAndGetOFStringArray'
    then
        echo "$i"
    fi
done