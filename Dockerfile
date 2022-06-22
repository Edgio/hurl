FROM ubuntu:latest
ENV DEBIAN_FRONTEND="noninteractive"
RUN apt update && apt install -y libssl-dev git cmake make g++
RUN git clone https://github.com/edgioinc/hurl.git /app
WORKDIR /app
RUN ./build_simple.sh
RUN cd build; make install
ENTRYPOINT ["hurl"]