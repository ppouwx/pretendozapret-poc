FROM ghcr.io/wiiu-env/devkitppc:latest

RUN dkp-pacman -Syu --needed --noconfirm wiiu-wups wiiu-wums

WORKDIR /project
COPY . .

RUN make
