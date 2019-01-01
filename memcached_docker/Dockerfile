FROM python:2.7.15-alpine 

ENTRYPOINT []

# 1. Use any image as your base image, or "scratch"
# 2. Add fwatchdog binary via https://github.com/openfaas/faas/releases/
# 3. Then set fprocess to the process you want to invoke per request - i.e. "cat" or "my_binary"

#ADD https://github.com/openfaas/faas/releases/download/0.8.9/fwatchdog /usr/bin
#RUN chmod +x /usr/bin/fwatchdog

RUN mkdir -p /home/app

RUN apk add --update alpine-sdk
RUN apk add build-base python-dev py-pip jpeg-dev zlib-dev

ENV LIBRARY_PATH=/lib:/usr/lib

RUN apk --no-cache add curl \
    && echo "Pulling watchdog binary from Github." \
    && curl -sSL https://github.com/openfaas/faas/releases/download/0.8.9/fwatchdog > /usr/bin/fwatchdog \
    && chmod +x /usr/bin/fwatchdog \
    && cp /usr/bin/fwatchdog /home/app \
    && apk del curl --no-cache
    
# Add non root user

RUN addgroup -S app && adduser app -S -G app
RUN chown app /home/app

COPY simple_memcached.py /home/app 
COPY requirements.txt /home/app

WORKDIR /home/app

USER app

RUN pip install --user --trusted-host pypi.python.org -r requirements.txt

# Populate example here - i.e. "cat", "sha512sum" or "node index.js"
ENV fprocess="python simple_memcached.py"
# Set to true to see request in function logs
ENV write_debug="false"

HEALTHCHECK --interval=5s CMD [ -e /tmp/.lock ] || exit 1
CMD [ "fwatchdog" ]
