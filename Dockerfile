FROM ubuntu:rolling

COPY ./ubuntu-requirements.txt /tmp/
RUN apt-get update && xargs -a /tmp/ubuntu-requirements.txt apt-get install -y

WORKDIR /project