FROM ubuntu:latest

RUN apt update && \
    apt install -y curl gnome-builder pulseaudio libadwaita-1-dev meson ninja-build curl cmake libpulse-dev

RUN curl -L https://github.com/microsoft/onnxruntime/releases/download/v1.14.1/onnxruntime-linux-x64-1.14.1.tgz | tar -xz -C /opt
RUN curl -L https://april.sapples.net/april-english-dev-01110_en.april -o /opt/april-english-dev-01110_en.april

ENV ONNX_ROOT=/opt/onnxruntime-linux-x64-1.14.1/
ENV LD_LIBRARY_PATH=/opt/onnxruntime-linux-x64-1.14.1/lib
ENV APRIL_MODEL_PATH=/opt/april-english-dev-01110_en.april

WORKDIR /app
#COPY . /app/

ENTRYPOINT [ "/app/entrypoint.sh" ]