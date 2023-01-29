#!/bin/bash

arch=$(uname -m)
compose_version=1.29.2

sudo apt-get update
sudo apt-get install \
    ca-certificates \
    curl \
    gnupg \
    lsb-release

sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

if [ "$arch" == 'x86_64' ]
then
    echo "x86_64 Architecture Detected"

    sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin
    sudo curl -L https://github.com/docker/compose/releases/download/$compose_version/docker-compose-`uname -s`-`uname -m` -o /usr/local/bin/docker-compose
    sudo chmod +x /usr/local/bin/docker-compose

    while getopts "nvidia_driver:" arg; do
      curl -s -L https://nvidia.github.io/nvidia-docker/gpgkey | sudo apt-key add -
      distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
      curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.list | sudo tee /etc/apt/sources.list.d/nvidia-docker.list
      sudo apt-get update
      sudo apt-get install -y nvidia-docker2
      sudo apt-get install -y nvidia-container-runtime
    done


elif [ "$arch" == 'aarch64' ]
then 
    echo "aarch64 Architecture Detected"
    echo "--> Experimental / Use with Caution"

    # Adlink roscube req
    sudo sed -i 's/debian/ubuntu/g' /etc/apt/sources.list.d/docker.list

    sudo apt-get update && sudo apt-get install -y \
      docker-ce:arm64 \
      docker-ce-cli \
      containerd.io \
      docker-compose \
      docker-compose-plugin

    sudo curl -L https://github.com/docker/compose/releases/download/$compose_version/docker-compose-`uname -s`-`uname -m` -o /usr/local/bin/docker-compose
    sudo chmod +x /usr/local/bin/docker-compose

else
    echo "Architechture Not Supported"
    exit 1
fi

sudo groupadd docker
sudo usermod -aG docker $USER
newgrp docker