# zmq + opencv + tbb example

## build

```sh
docker build --tag zmq_cv .
```

## cli

```sh
docker run \
  --rm -ti \
  -v `pwd`:/data \
  zmq_cv \
    /opt/identity-docker/build/identity \
      --input /data/a.mp4 \
      --output /data/b.mp4
```


## zmq

```sh
docker volume rm data
docker volume create data
docker volume inspect data
sudo mkdir -p /var/lib/docker/volumes/data/_data/in/
sudo mkdir -p /var/lib/docker/volumes/data/_data/out/
sudo tree /var/lib/docker/volumes/data/_data
docker run \
  -ti --rm \
  --volume data:/data \
  --name mp4player \
  vca_frame_grabber-x64 \
    /bin/bash
env END_CONDITION=/data/stop ./mp4player push
```

```sh
docker run \
  --rm -ti \
  -v data:/data --net=container:mp4player \
  -e ZMQ_FRAME_GRABBER_ENDPOINT="ipc://@/scorer/frame_grabber-video0" \
  -e INACTIVITY_TIMEOUT=10 \
  -e OUTPUT_DIR="/data/out" \
  zmq_cv \
    /opt/identity-docker/identity.bash pull one-shot
```

```sh
sudo cp a.mp4 /var/lib/docker/volumes/data/_data/in/video0_2017-10-17_17:04:10+0900.mp4
sudo tree /var/lib/docker/volumes/data/_data
```

## debug memo

```sh
docker run \
  --rm -ti \
  -v data:/data --net=container:mp4player \
  -v `pwd`:/opt/zmq-cv-dev/ \
  --workdir /opt/zmq-cv-dev/ \
  zmq_cv \
    bash

mkdir -p /opt/zmq-cv-dev/build/ && pushd /opt/zmq-cv-dev/build/ && cmake .. && make -j && popd

./build/zmq_identity \
  --socktype="PULL" \
  --endpoint="ipc://@/scorer/frame_grabber-video0" \
  --timeout=10 \
  --output_video="/opt/zmq-cv-dev/result.mp4" \
  --output_log="/opt/zmq-cv-dev/result.tsv"
```

```sh
scp -i ~/google2.ssh ubuntu@${HOST}:/home/ubuntu/zmq-cv/b.mp4 ./
```

```sh
rsync \
  --cvs-exclude \
  --exclude build --exclude *.mp4 \
  -ahrv --delete --stats --progress \
  -e "ssh -i ~/google2.ssh" \
  ./ \
  ubuntu@${HOST}:/home/ubuntu/zmq-cv/
```

-n で dry run
