# Skywing Docker 

## Host Installation 

Clone the source repo and it's dependancies : 
    
    git clone --recurse-submodules --branch docker https://github.com/mvancleaver/Skywing.git

#### Setup
##### Manual 
- Install [Docker Engine], [Docker Compose], and any dependanceies as outlined in the Docs

##### Automatic (Trust, but verify)
Use the Installer Script : [`./docker/init_host.sh`](https://github.com/mvancleaver/Skywing/blob/docker/docker/init_host.sh)

**General Usage** : 
    
    ./docker/init_host.sh


- **GPU Enbabled** :[`./docker/init_host.sh -nvidia_driver`]


## Docker Usage

#### Images
- `Images` are built using the `Dockerfile` below :

| Image Name                                | Dockerfile                  | Description           |
|---                                        |---                          |---                    |
| **`empyreanlattice/skywing:latest`**      | [Dockerfile.skywing]        | Base Skywing Build    |



#### Services
- `Services` are defined in [`./docker/cookbook/docker-compose.yml`](https://github.com/mvancleaver/Skywing/blob/docker/docker/cookbook/docker-compose.yml) :

| Service Name                    | Description                                   |
|---                              |---                                            |
| **`Skywing`**                   | Base Skywing Build with TTY Access            |

#### Building Services 
- Services can be built using : `docker-compose build 'Service Name' `

#### Runtime
- Services can be run using : `docker-compose up 'Service Name' `

## Tested Configurations

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



[Dockerfile.skywing]:https://github.com/mvancleaver/Skywing/blob/docker/docker/Dockerfile.skywing
[Docker Engine]:https://docs.docker.com/engine/install/ubuntu/
[Docker Compose]:https://docs.docker.com/compose/install/linux/#install-the-plugin-manually