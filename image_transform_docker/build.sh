docker build -t transform_image .
docker tag transform_image lambdanic/transform_image:latest
docker push lambdanic/transform_image:latest
