FROM ubuntu:rolling

COPY ./ubuntu-requirements.txt /tmp/
RUN apt-get update && xargs -a /tmp/ubuntu-requirements.txt apt-get install -y
RUN rm /tmp/ubuntu-requirements.txt

COPY ./python-requirements.txt /tmp/
RUN pip3 install -r /tmp/python-requirements.txt
RUN rm /tmp/python-requirements.txt

WORKDIR /project