for i in $(find ~/.conan/data -name \*.a); do # Not recommended, will break on whitespace
    if nm -u "$i" | grep 'secure_getenv\|__fdelt_chk'
    then
        echo "$i"
    fi
done
