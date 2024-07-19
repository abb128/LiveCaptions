```
curl -L https://github.com/microsoft/onnxruntime/releases/download/v1.14.1/onnxruntime-linux-x64-1.14.1.tgz | tar -xz -C $(pwd)
curl -L https://april.sapples.net/april-english-dev-01110_en.april -o $(pwd)/april-english-dev-01110_en.april

docker build -t livecaptions-builder .
docker run --rm --name livecaptions-builder -it -v $(pwd):/app livecaptions-builder
```

Then you can run it with:
```
ONNX_ROOT=$(pwd)/onnxruntime-linux-x64-1.14.1/ \
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/onnxruntime-linux-x64-1.14.1/lib \
APRIL_MODEL_PATH=$(pwd)/april-english-dev-01110_en.april \
GSETTINGS_SCHEMA_DIR=$(pwd)/builddir/data \
./builddir/src/livecaptions
```