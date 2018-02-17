#!/bin/bash
PROG=`basename $0`


usage()
{
cat >&2 <<EOF
Usage:
  $PROG {-h|--help}
  $PROG [one-shot] [force]

Options:
  -h,--help Show this usage

  sub       Ingest image frames from the lossy pub/sub channel (default)
  pull      Ingest image frames from the lossless push/pull channel

  one-shot  Exit after playing the first batch of MP4 files
  force     Do not exit when \$HOME/stop file is found while starting

Example:
  $PROG one-shot
EOF
}


# Handle the arguments
ONE_SHOT=
FORCE=
ZMQ_FRAME_GRABBER_SOCKTYPE=

while [ $# -gt 0 ];
do
    case "$1" in
        one-shot) ONE_SHOT=true;;
        force) FORCE=force;;
        sub) ZMQ_FRAME_GRABBER_SOCKTYPE=SUB ;;
        pull) ZMQ_FRAME_GRABBER_SOCKTYPE=PULL ;;
        -h|--help) usage; exit 0;;
        *) usage; exit 1;;
    esac
    shift
done

if [ "$ONE_SHOT" != true -a "$FORCE" = "force" -a -z "$END_CONDITION" ]; then
    echo "**** WARNING: Running forever unless interrupted. ****"
fi

[ "$ONE_SHOT" != true ] || echo "Running in one-shot mode..."



# The base directory to dump the output
OUTPUT_DIR=${OUTPUT_DIR:-/data/out}

# The completion file to notify all tasks are over
COMPLETION_FILE=${COMPLETION_FILE:-/data/completed.$PROG}

# The lock file to prevent multiple instances from running
MUTEX=/var/run/lock/$PROG

# Set the ZeroMQ frame emitting endpoint
[ -n "$ZMQ_FRAME_GRABBER_ENDPOINT" ] ||
ZMQ_FRAME_GRABBER_ENDPOINT="ipc://@/scorer/frame_grabber-video0"

# Stop the tracker after this amount of time passes without incoming frames
INACTIVITY_TIMEOUT=${INACTIVITY_TIMEOUT:-30}

# Define the loop condition
keep_looping()
{
    # return 0 to continue the loop; return 1 otherwise

    [ "$1" = "force" ] && return 0
    [ -e "$HOME/stop" ] && { echo "Quitting by stop file." >&2; return 1; }
    return 0
}



main_func()
{
    while keep_looping "$FORCE"
    do
        echo "$(date '+%Y-%m-%d_%H:%M:%S%z') Starting the tracker ..."
        time ./build/zmq_identity \
          --socktype=$ZMQ_FRAME_GRABBER_SOCKTYPE \
          --endpoint=$ZMQ_FRAME_GRABBER_ENDPOINT \
          --timeout=$INACTIVITY_TIMEOUT \
          --output_video="./result.mp4" \
          --output_log="./result.tsv"

        sleep 10
        RETVAL=$?
        echo $RETVAL

        if [ $RETVAL -eq 0 ]; then

          export FILE_NAME=video0_$(head -n1 result.tsv | sed -e 's/\s/_/g')

          mkdir -p $OUTPUT_DIR/videos
          mkdir -p $OUTPUT_DIR/texts
          mv ./result.mp4 $OUTPUT_DIR/videos/$FILE_NAME.mp4
          mv ./result.tsv $OUTPUT_DIR/texts/$FILE_NAME.tsv
          tree $OUTPUT_DIR
        fi


        echo "$(date '+%Y-%m-%d_%H:%M:%S%z') Tracker ended. ($RETVAL)"

        [ -z "$END_CONDITION" -o ! -e "$END_CONDITION" ] || {
            echo "$END_CONDITION exists"
            break
        }

        [ "$ONE_SHOT" = "true" ] && break

    done
}



# Only one instance of uploader can run at once
exec 9>"$MUTEX"
flock --exclusive --nonblock 9
[ $? -eq 0 ] || exit

# Okay, we have the lock


# Prepare for the completion file
rm -f "$COMPLETION_FILE"
trap "touch $COMPLETION_FILE" 0

main_func


# Release the lock
exec 9>&-

exit $RETVAL

# End.
