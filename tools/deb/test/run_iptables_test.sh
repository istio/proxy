#!/bin/bash

# Test 'istio-iptables' script, verify the rules generate the expected iptables.

# This is a 'unit' test - it doesn't check that kernel and sidecar capture the traffic.
# Script requires a working docker on the test machine
# It is run in the proxy dir, will create a docker image with proxy deb installed

export DOCKER_IMAGE=${DOCKER_IMAGE:-rawvm_test}
tools/deb/test/build_docker.sh

DOCKER_NAME=${DOCKER_NAME:-iptest}

function compare_output() {
    EXPECTED=$1
    RECEIVED=$2
    USER=$3

    diff $EXPECTED $RECEIVED &>/dev/null
    if [ $? -gt 0 ]
    then
        echo "Output $RECEIVED does not match expected $EXPECTED "
        diff $EXPECTED $RECEIVED
        return 1
    else
        return 0
    fi
}

if [[ ${1:-} == "run" ]] ; then
  # Start a docker container - needs priviledged for iptable manipulation.
  DTAG=$(docker run --name ${DOCKER_NAME} -d -v `pwd`:/ws/proxy --privileged ${DOCKER_IMAGE}  /bin/bash -c "trap : TERM INT; sleep infinity & wait")

  # trap "docker rm -f ${DTAG}" EXIT

  # Driver to run the commands in the container. Kube exec could be used instead.
  # RUN="docker exec -i ${DOCKER_NAME}  /bin/bash -c "

  echo "Access the docker images as: docker exec -it ${DTAG} /bin/bash"
  echo "Make sure to remove it when done: docker rm -f ${DTAG}"
  exit 0

else
  # Start a docker container - needs priviledged for iptable manipulation.

  DTAG=$(docker run --name ${DOCKER_NAME} -v `pwd`:/ws/proxy --privileged ${DOCKER_IMAGE} /ws/proxy/tools/deb/test/iptables_tests.sh )
  docker rm ${DOCKER_NAME}

  compare_output tools/deb/test/golden.in test.logs/real.in
  compare_output tools/deb/test/golden.cidr test.logs/real.cidr
  compare_output tools/deb/test/golden.defaults test.logs/real.defaults
  compare_output tools/deb/test/golden.exclude test.logs/real.exclude

fi


