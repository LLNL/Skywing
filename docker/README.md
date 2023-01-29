# Skywing Docker 

### Host Installation 

Clone the source repo and it's dependancies : 
    
    git clone --recurse-submodules --branch docker https://github.com/mvancleaver/Skywing.git
    cd Skywing/
    

#### Setup
##### Manual 
* Install [Docker Engine], [Docker Compose], and any dependanceies as outlined in the Docs. This method is advised if you need to enable support for specialized devices/drivers ie. gpu / architecture / networks.

[Docker Engine]:https://docs.docker.com/engine/install/ubuntu/
[Docker Compose]:https://docs.docker.com/compose/install/linux/#install-the-plugin-manually

##### x86 Simple Automatic  
* Use the Installer Script : [`./docker/init_host.sh`](https://github.com/mvancleaver/Skywing/blob/docker/docker/init_host.sh)


### Docker Usage
Basic build/run/lauch functionality is built around `docker-compose`

#### Images
* A pre-built `Image` is available from the following :

| Image Name                                                                                    | Dockerfile                  | Description                                 |
|---                                                                                            |---                          |---                                          |
| [**`empyreanlattice/skywing:latest`**](https://hub.docker.com/r/empyreanlattice/skywing)      | [Dockerfile.skywing]        | Skywing Build with Examples / Tests         |

[Dockerfile.skywing]:https://github.com/mvancleaver/Skywing/blob/docker/docker/Dockerfile.skywing

#### Services
* `Services` are defined in [`./docker/cookbook/docker-compose.yml`](https://github.com/mvancleaver/Skywing/blob/docker/docker/cookbook/docker-compose.yml) :

| Service Name                        | Description                                                     |
|---                                  |---                                                              |
| **`skywing_src`**                   | Base Skywing Env with TTY Access pulled from Dockerhub          |
| **`skywing_dev`**                   | Base Skywing Env with TTY Access built from local Dockerfile    |

##### Running Services
* Attach to a Skywing Container Shell : 
```
    docker-compose run skywing_src /bin/bash
```

##### Building Custom Images
* Custom images can be built via making the appropriate changes to [Dockerfile.skywing] and running : 
```
    docker-compose build skywing_dev 
```

### End-to-End Installation 

    # Clone the Source Repo and Submodules 
    git clone --recurse-submodules --branch docker https://github.com/mvancleaver/Skywing.git

    # Initize the Host System 
    ./Skywing/docker/init_host.sh

    # Attach to a Skywing Enviorment Container  
    cd Skywing/docker/cookbook && docker-compose run skywing_src /bin/bash

____________________________


### Tested Configurations

| -                  | x86 Desktop        | x86 Laptop          | Adlink Roscube AGX  | Jetson Orin Dev Kit   |
|---                 |---                 |---                  |---                  |---                    |
| **OS**             | Ubuntu 18.04       | Ubuntu 20.04        | Ubuntu 18.04        | Ubuntu 20.04          |
| **Kernel**         | 5.4.0-137-generic  |                     | 4.9.201-rqx580      |                       |
| **CPU**            | Intel i9-9900K     | Intel i7-12800H     | ARM 8C Carmel       | ARM 12C Cortex-A78AE  |
| **GPU**            | RTX 2080 Ti        | RTX A4500           | Volta - 512 Core    | Ampere - 2048 Core    |
| **GPU Driver**     | 525.60.11          |                     | 10.2.89             |                       |
| **CUDA Arch**      | 12.0               |                     | 7.2                 |                       |
| **Docker Engine**  | 20.10.22           |                     | 20.10.2             |                       |
| **Docker Compose** | 1.29.2             |                     | 1.29.2              |                       |



