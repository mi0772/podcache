# PodCache

PodCache is a high-performance, multi-partition in-memory cache server with automatic disk overflow, designed for containerized applications. It implements the Redis RESP protocol and provides transparent hybrid storage (memory + disk) with LRU eviction policies.

## Features

- **High Performance**: Thread-safe multi-partition LRU cache with O(1) operations
- **Hybrid Storage**: Automatic overflow from memory to disk when capacity is exceeded
- **Redis Compatible**: Implements Redis RESP protocol for easy integration
- **Content Addressable Storage (CAS)**: SHA256-based disk storage for data integrity
- **Multi-Partition Architecture**: Configurable partitions to reduce lock contention
- **Comprehensive Logging**: Detailed logging system for monitoring and debugging
- **Containerization Ready**: Docker support with supervisor configuration
- **Signal Handling**: Graceful shutdown on SIGINT/SIGTERM

## Architecture

PodCache uses a three-tier caching architecture:

1. **Memory Layer**: Fast LRU caches partitioned by key hash
2. **Disk Layer**: Content-addressable storage (CAS) for overflow data
3. **Protocol Layer**: TCP server implementing Redis RESP protocol

### Key Components

- **Pod Cache**: Main orchestration layer managing partitions and disk overflow
- **LRU Cache**: In-memory hash table + doubly-linked list implementation
- **CAS Registry**: Content-addressable storage with SHA256-based file organization
- **RESP Parser**: Redis protocol parser for command processing
- **TCP Server**: Multi-threaded server with client connection handling

## Supported Commands

- `SET key value` - Store a key-value pair
- `GET key` - Retrieve value by key
- `DEL key` - Delete a key
- `INCR key` - Increment numeric value
- `CLIENT` - Client connection management
- `PING` - Connection health check

## Installation

### Prerequisites

- CMake 3.16 or higher
- OpenSSL 3.x
- GCC or Clang compiler
- netcat (for testing)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/mi0772/podcache.git
cd podcache

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
make

# The binary will be available as ./podcache
```

### macOS Installation

```bash
# Install OpenSSL via Homebrew
brew install openssl@3

# Build with OpenSSL path
cmake -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3 ..
make
```

## Configuration

PodCache is configured via environment variables:

| Variable               | Default | Range      | Description                     |
| ---------------------- | ------- | ---------- | ------------------------------- |
| `PODCACHE_SIZE`        | 100     | 1-4096     | Cache size in MB                |
| `PODCACHE_SERVER_PORT` | 6379    | 1024-65535 | TCP server port                 |
| `PODCACHE_PARTITIONS`  | 1       | 1-64       | Number of cache partitions      |
| `PODCACHE_FSROOT`      | "./"    | -          | Root directory for disk storage |

## Usage

### Basic Usage

```bash
# Start server with default settings (100MB cache, port 6379)
./podcache

# Start with custom configuration
PODCACHE_SIZE=256 PODCACHE_SERVER_PORT=6380 ./podcache
```

### Client Examples

```bash
# Using netcat for testing
echo -e '*3\r\n$3\r\nSET\r\n$5\r\nhello\r\n$5\r\nworld\r\n' | nc localhost 6379
echo -e '*2\r\n$3\r\nGET\r\n$5\r\nhello\r\n' | nc localhost 6379
```

### Redis CLI Compatible

```bash
# PodCache is compatible with redis-cli
redis-cli -p 6379 SET mykey "Hello PodCache"
redis-cli -p 6379 GET mykey
```

## Testing

The project includes comprehensive test suites:

### Simple Test

```bash
./simple_test.sh
```

### Disk Swapping Test

```bash
./test_disk_swap.sh
```

### Medium Load Test

```bash
./medium_test.sh
```

### Manual Testing

```bash
# Test basic functionality
./test_logging.sh
```

## How It Works

### Memory Management

1. **Partitioned Storage**: Keys are hashed and distributed across multiple LRU partitions
2. **Capacity Management**: Each partition has a fixed memory capacity
3. **LRU Eviction**: When a partition is full, least recently used items are moved to disk

### Disk Overflow

1. **Automatic Spillover**: When memory is full, tail elements are written to disk
2. **CAS Storage**: Files are stored using SHA256-based directory structure
3. **Transparent Retrieval**: Disk items are automatically promoted back to memory on access
4. **Cleanup**: Promoted items are removed from disk to prevent duplication

### Directory Structure

Disk storage uses a content-addressable structure:

```
FSROOT/
├── ab12/
│   └── cd34/
│       └── ef56/
│           └── 7890/
│               ├── value.dat
│               └── time.dat
```

## Performance Characteristics

- **Memory Operations**: O(1) average time complexity
- **Disk Operations**: O(1) with filesystem cache
- **Thread Safety**: Fine-grained locking per partition
- **Scalability**: Linear performance scaling with partition count

## Docker Support

PodCache provides comprehensive Docker support with pre-built base images for easy integration with microservices.

### Pre-built Base Image

PodCache is available as a pre-built Docker base image that can be used in multi-stage builds:

```dockerfile
# Pull the base image containing PodCache
FROM mi0772/podcache-base:latest AS podcache

# Your application stage
FROM openjdk:17-jdk-slim AS app
# ... your application setup ...

# Copy PodCache from base image
COPY --from=podcache /usr/local/bin/podcache /usr/local/bin/podcache
COPY --from=podcache /etc/podcache/ /etc/podcache/
COPY --from=podcache /etc/supervisor/conf.d/podcache.conf /etc/supervisor/conf.d/
```

### Microservice Integration Example

Here's a complete example of integrating PodCache with a Spring Boot microservice:

```dockerfile
# Multi-stage build with PodCache integration
FROM mi0772/podcache-base:latest AS podcache
FROM openjdk:17-jdk-slim AS spring-app

# Install supervisor and runtime dependencies
RUN apt-get update && apt-get install -y \
    supervisor \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy PodCache from base image
COPY --from=podcache /usr/local/bin/podcache /usr/local/bin/podcache
COPY --from=podcache /etc/podcache/ /etc/podcache/
COPY --from=podcache /etc/supervisor/conf.d/podcache.conf /etc/supervisor/conf.d/

# Create podcache user and directories
RUN useradd -r -s /bin/false podcache && \
    mkdir -p /var/lib/podcache /var/log && \
    chown podcache:podcache /var/lib/podcache

# Copy your Spring Boot application
COPY target/myapp.jar /app/app.jar

# Supervisor configuration for both services
COPY docker/supervisord-full.conf /etc/supervisor/supervisord.conf

# Health check for both services
HEALTHCHECK --interval=30s --timeout=10s --start-period=30s --retries=3 \
    CMD curl -f http://localhost:6379/health && \
        curl -f http://localhost:8080/actuator/health || exit 1

EXPOSE 8080 6379

CMD ["/usr/bin/supervisord", "-c", "/etc/supervisor/supervisord.conf"]
```

### Supervisor Configuration

The multi-service setup uses Supervisor to manage both PodCache and your application:

```ini
[supervisord]
nodaemon=true
user=root
logfile=/var/log/supervisord.log
pidfile=/var/run/supervisord.pid

[program:podcache]
command=/usr/local/bin/podcache --config /etc/podcache/podcache.conf
user=podcache
autostart=true
autorestart=true
stderr_logfile=/var/log/podcache.err.log
stdout_logfile=/var/log/podcache.out.log
priority=100

[program:spring-app]
command=java -jar /app/app.jar
user=root
autostart=true
autorestart=true
stderr_logfile=/var/log/spring.err.log
stdout_logfile=/var/log/spring.out.log
priority=200
environment=PODCACHE_HOST="localhost",PODCACHE_PORT="6379"
```

### Configuration Options

Environment variables available in Docker containers:

| Variable               | Default             | Description                     |
| ---------------------- | ------------------- | ------------------------------- |
| `PODCACHE_SIZE`        | 100                 | Cache size in MB                |
| `PODCACHE_SERVER_PORT` | 6379                | TCP server port                 |
| `PODCACHE_PARTITIONS`  | 1                   | Number of cache partitions      |
| `PODCACHE_FSROOT`      | "/var/lib/podcache" | Root directory for disk storage |

### Running with Docker Compose

Example `docker-compose.yml` for development:

```yaml
version: "3.8"
services:
  app-with-cache:
    build: .
    ports:
      - "8080:8080"
      - "6379:6379"
    environment:
      - PODCACHE_SIZE=256
      - PODCACHE_SERVER_PORT=6379
      - SPRING_PROFILES_ACTIVE=docker
    volumes:
      - podcache_data:/var/lib/podcache
      - ./logs:/var/log
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/actuator/health"]
      interval: 30s
      timeout: 10s
      retries: 3

volumes:
  podcache_data:
```

### Building Your Own Base Image

If you need to customize PodCache, you can build your own base image:

```dockerfile
# Build stage
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y \
    build-essential cmake libssl-dev pkg-config
WORKDIR /app
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release

# Runtime stage
FROM ubuntu:22.04 AS podcache-base
RUN apt-get update && apt-get install -y \
    libc6 supervisor curl ca-certificates && \
    rm -rf /var/lib/apt/lists/* && \
    useradd -r -s /bin/false podcache

COPY --from=builder /app/build/podcache /usr/local/bin/podcache
RUN chmod +x /usr/local/bin/podcache && \
    mkdir -p /var/lib/podcache /etc/podcache /var/log && \
    chown podcache:podcache /var/lib/podcache

EXPOSE 6379
CMD ["/usr/local/bin/podcache"]
```

### Kubernetes Deployment

Example Kubernetes deployment with PodCache sidecar:

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: myapp-with-podcache
spec:
  replicas: 3
  selector:
    matchLabels:
      app: myapp
  template:
    metadata:
      labels:
        app: myapp
    spec:
      containers:
        - name: app
          image: myapp:latest
          ports:
            - containerPort: 8080
          env:
            - name: CACHE_HOST
              value: "localhost"
            - name: CACHE_PORT
              value: "6379"
        - name: podcache
          image: mi0772/podcache-base:latest
          ports:
            - containerPort: 6379
          env:
            - name: PODCACHE_SIZE
              value: "512"
            - name: PODCACHE_PARTITIONS
              value: "4"
          resources:
            requests:
              memory: "512Mi"
              cpu: "250m"
            limits:
              memory: "1Gi"
              cpu: "500m"
```

### Benefits of Docker Integration

1. **Zero Configuration**: Pre-configured and ready to use
2. **Multi-Stage Builds**: Efficient container size optimization
3. **Service Orchestration**: Supervisor manages both cache and application
4. **Health Monitoring**: Built-in health checks for container orchestration
5. **Production Ready**: Optimized for containerized deployments
6. **Microservice Pattern**: Perfect sidecar cache for distributed architectures

## Logging

PodCache provides comprehensive logging with configurable levels:

- **DEBUG**: Detailed operation tracing
- **INFO**: Normal operational messages
- **WARN**: Warning conditions
- **ERROR**: Error conditions

Logs include:

- Cache operations (PUT/GET/EVICT)
- Memory usage statistics
- Disk I/O operations
- Client connections
- Server status

## Development

### Project Structure

```
podcache/
├── src/           # Source code
├── include/       # Header files
├── tests/         # Test files
├── docker/        # Docker configuration
├── build/         # Build artifacts
└── cmake-build-debug/  # Debug build
```

### Key Files

- `main.c` - Application entry point
- `pod_cache.c` - Main cache orchestration
- `lru_cache.c` - In-memory LRU implementation
- `cas.c` - Content-addressable storage
- `server_tcp.c` - TCP server and protocol handling
- `resp_parser.c` - Redis protocol parser

## License

MIT License - see LICENSE file for details

## Author

Carlo Di Giuseppe

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Roadmap

- Persistence layer improvements
- Cluster support
- Advanced eviction policies
- Metrics and monitoring endpoints
- Configuration file support
- Memory-mapped file optimization
