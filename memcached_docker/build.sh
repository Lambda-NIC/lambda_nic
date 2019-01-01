docker build -t simple_memcached .
docker tag simple_memcached lambdanic/simple_memcached:latest
docker push lambdanic/simple_memcached:latest
