docker build -t slideio-dev \
--build-arg CONAN_LOGIN_USERNAME=${CONAN_LOGIN_USERNAME} \
--build-arg CONAN_PASSWORD=${CONAN_PASSWORD} \
--build-arg CONAN_SERVER_URL=${CONAN_SERVER_URL} \
./docker/alpine


