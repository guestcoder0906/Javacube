# Previous code remains unchanged until line 60
gcc -pthread -O2 -o verify_build verify_build.c \
    -I cubiomes -L cubiomes -lcubiomes -lm

echo "Compilation done."

echo "Running verify_build..."
./verify_build
