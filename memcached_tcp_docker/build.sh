docker build -t simple_memcached_tcp .
docker tag simple_memcached_tcp lambdanic/simple_memcached_tcp:latest
docker push lambdanic/simple_memcached_tcp:latest
