docker build -t slideio-bin --no-cache \
--build-arg CONAN_SERVER_URL=${CONAN_SERVER_URL} \
./docker/debian
docker tag slideio-bin booritas/slideio-debian-bin:2.8.1b

