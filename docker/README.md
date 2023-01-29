# Skywing Docker 

## Host Installation 

### Setup Scripts
Prepare the host by running one of the following commands :
- **General Usage** : [`./docker/init_host.sh`](https://duckduckgo.com)
- **GPU Enbabled** :[`./docker/init_host.sh -nvidia_driver`](https://duckduckgo.com)


## Docker Usage

#### Images
- `Images` are built using the `Dockerfile` below :

| Image Name                      | Dockerfile                                    | Description                                   |
|---                              |---                                            |---                                            |
| **`empyreanlattice/skywing`**   | Dockerfile.skywing                            | Base Skywing Image                            |

#### Services
- `Services` defined in `./docker/cookbook/docker-compose.yml` :

| Service Name                    | Description                                   |
|---                              |---                                            |
| **`skywing`**                   | Base Skywing Build with TTY Access            |
| **`tutorial_introduction`**     | Builds + Runs the Introduction Tutorial       |
| **`tutorial_pubsub`**           | Builds + Runs the Pub-Sub Tutorial            |
| **`tutorial_itr_method`**       | Builds + Runs the Iterative Methods Tutorial  |

#### Building Services 
- Services can be built using : `docker-compose build 'Service Name' `

#### Runtime
- Services can be run using : `docker-compose up 'Service Name' `

### Tested Configurations

| -                  | x86 Desktop        | x86 Laptop          | Adlink AGX          | Jetson Orin           |
|---                 |---                 |---                  |---                  |---                    |
| **OS**             | Ubuntu 18.04       | Ubuntu 20.04        | Ubuntu 18.04        | Ubuntu 20.04          |
| **Kernel**         | 5.4.0-137-generic  |                     | 4.9.201-rqx580      |                       |
| **CPU**            | Intel i9-9900K     | Intel i7-12800H     | ARM 8C Carmel       | ARM 12C Cortex-A78AE  |
| **GPU**            | Nvidia RTX 2080 Ti | NVIDIA® RTX A4500   | Volta - 512C        | Ampere - 2048C        |
| **GPU Driver**     | 525.60.11          |                     | 10.2.89             |                       |
| **CUDA Arch**      | 12.0               |                     | 7.2                 |                       |
| **Docker Engine**  | 20.10.22           |                     | 20.10.2             |                       |
| **Docker Compose** | 1.29.2             |                     |                     |                       |


