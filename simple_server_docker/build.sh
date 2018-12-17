docker build -t simple_server .
docker tag simple_server lambdanic/simple_server:latest
docker push lambdanic/simple_server:latest
