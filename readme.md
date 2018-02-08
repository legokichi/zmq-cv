```sh
docker build --tag a .
docker run \
  --rm -ti \
  -v `pwd`:/data \
  a \
    /opt/identity-docker/build/identity \
      --input /data/a.mp4 \
      --output /data/b.mp4
```