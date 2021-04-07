OUTDIR=artifacts
mkdir -p ${OUTDIR}
docker run -i --rm -v "${PWD}/${OUTDIR}:/mnt/artifacts" -v "${PWD}:/app" rmkit /bin/bash << COMMANDS
cd /app
ls rmkit/src/build/rmkit.h
make
mkdir -p build/xournal
cp -r build/symbiote /mnt/artifacts/

# to make "make clean" work outside docker
chown -R $(id -u):$(id -u) build
chown -R $(id -u):$(id -u) /mnt/artifacts
COMMANDS

