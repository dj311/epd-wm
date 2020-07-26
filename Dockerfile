FROM archlinux:latest

RUN pacman -Syu --noconfirm
RUN pacman -S awk file git base-devel --noconfirm

# We're are using the arch package to fill in all the dependencies, but we'll build it our selves.

# Avoid bullshit "don't run as root" babysitting
RUN useradd --no-create-home --shell=/bin/false build && usermod -L build
RUN echo "build ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
RUN echo "root ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
USER build

RUN cd /tmp && git clone https://aur.archlinux.org/wlroots-git.git
RUN cd /tmp/wlroots-git && makepkg --syncdeps --noconfirm

# Back to root
USER root

WORKDIR /project