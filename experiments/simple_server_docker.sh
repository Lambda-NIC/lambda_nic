for run in {1..10000}
do
  curl http://172.24.90.32:8001/api/v1/namespaces/openfaas/services/http:gateway:/proxy/function/simpleserver
done
