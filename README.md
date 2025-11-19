# Simple UTC Daemon

A lightweight, high-performance UTC (Universal Time Coordinate) daemon written in C++.

## Features

- **High Performance**: Optimized for low-latency time synchronization
- **Cross-Platform**: Supports macOS, Linux, and Windows
- **Configurable**: Flexible configuration options (INI, JSON, YAML formats)
- **Secure**: Comprehensive security features including authentication, ACLs, rate limiting, and DDoS protection
- **Containerized**: Full Docker support for development and deployment
- **Version 0.3.0**: Enhanced security with authentication (MD5/SHA-1/SHA-256), access control lists, rate limiting, and DDoS protection

## Quick Start

### Using Docker (Recommended)

The fastest way to get started is with Docker:

```bash
# Quick deployment
cd deployment/examples/docker
docker-compose up -d

# Development environment
docker-compose --profile dev up dev

# Build for all platforms
./scripts/build-docker.sh -d all

# Deploy with custom settings
./scripts/deploy-docker.sh -p runtime -c ./config -l ./logs
```

For more Docker options, see the [Docker Deployment Guide](docs/deployment/docker.md).

### Local Development

```bash
# Install dependencies
make deps

# Build the project
make build

# Run tests
make test

# Install
make install
```

## Help System

The project includes a comprehensive modular help system:

```bash
# Quick reference
make help

# Complete reference
make help-all

# Category-specific help
make help-build       # Build and development targets
make help-package     # Package creation targets
make help-deps        # Dependency management targets
make help-service     # Service management targets
make help-docker      # Docker targets
make help-config      # Configuration management targets
make help-platform    # Platform-specific targets
```

## Docker Infrastructure

### Multi-Platform Support

The Docker setup supports multiple Linux distributions and architectures:

- **Distributions**: Ubuntu, CentOS, Alpine Linux
- **Architectures**: x86_64, arm64, armv7
- **Multi-stage builds** for optimized production images
- **Health checks** and monitoring capabilities
- **Volume mounts** for configuration and logs

### Quick Docker Commands

```bash
# Development environment
docker-compose --profile dev up dev

# Runtime container
docker-compose --profile runtime up simple-utcd

# Build for specific distribution
docker-compose --profile build build build-ubuntu
docker-compose --profile build build build-centos
docker-compose --profile build build build-alpine
```

### Automated Building

Use the build script for automated cross-platform building:

```bash
# Build for all distributions
./scripts/build-docker.sh -d all

# Build for specific distribution
./scripts/build-docker.sh -d ubuntu

# Clean build cache
./scripts/build-docker.sh --clean
```

### Deployment Scripts

```bash
# Deploy runtime environment
./scripts/deploy-docker.sh -p runtime

# Deploy development environment
./scripts/deploy-docker.sh -p dev

# Force rebuild and deploy
./scripts/deploy-docker.sh --clean --force
```

For comprehensive Docker documentation, see [Docker Deployment Guide](docs/deployment/docker.md).

## Development

### Prerequisites

- CMake 3.15+
- C++17 compatible compiler
- OpenSSL
- JsonCPP

### Development Tools

```bash
# Install development tools (macOS)
make dev-deps

# Install development tools (Homebrew alternative)
make dev-deps-brew

# Run code quality checks
make format
make lint
make security-scan
```

### Testing

```bash
# Run tests
make test

# Run tests with verbose output
make test-verbose

# Run tests in debug mode
make dev-test
```

## Building

### Local Build

```bash
# Standard build
make build

# Debug build
make debug

# Release build
make release

# Development build
make dev-build
```

### Package Creation

```bash
# Create platform-specific packages
make package

# Create source package
make package-source

# Create all package formats
make package-all
```

## Service Management

### Installation

```bash
# Install system service
make service-install

# Check service status
make service-status

# Start service
make service-start

# Stop service
make service-stop
```

### Platform Support

- **macOS**: launchd service
- **Linux**: systemd service
- **Windows**: Windows Service

## Configuration

Simple UTC Daemon supports multiple configuration formats:

- **INI format** (`.conf` files) - Traditional configuration format
- **JSON format** (`.json` files) - Structured configuration
- **YAML format** (`.yaml` files) - Human-readable structured format

Configuration files are auto-detected based on file extension.

### Configuration Examples

```bash
# Basic configuration
config/examples/simple/simple-utcd.conf.example
config/examples/simple/simple-utcd.json
config/examples/simple/simple-utcd.yaml

# Security configurations (v0.3.0)
config/examples/security/authentication.conf
config/examples/security/acl.conf
config/examples/security/rate-limiting.conf

# Production configurations
config/examples/production/enterprise.conf
config/examples/production/cloud.conf

# Advanced configurations
config/examples/advanced/high-security.conf
config/examples/advanced/high-performance.conf
config/examples/advanced/load-balanced.conf
```

### Installation

```bash
# Install configuration files
make config-install

# Backup configuration
make config-backup

# Install log rotation
make log-rotate
```

### Environment Variables

Configuration values can be overridden using environment variables:

```bash
# Authentication key
export SIMPLE_UTCD_AUTH_KEY="your-secret-key"

# Other configuration overrides
export SIMPLE_UTCD_LISTEN_PORT=37
export SIMPLE_UTCD_LOG_LEVEL=INFO
```

## Security

Simple UTC Daemon includes comprehensive security features (Version 0.3.0):

### Authentication
- **Multiple Algorithms**: MD5, SHA-1, and SHA-256 authentication
- **Key Management**: Secure key storage and rotation support
- **Session Management**: Configurable session timeouts
- **Lockout Protection**: Automatic lockout after failed attempts

### Access Control Lists (ACL)
- **IP-based Restrictions**: Allow or deny specific IP addresses
- **CIDR Support**: Network-based restrictions using CIDR notation
- **Rule-based ACL**: Flexible rule configuration with priorities
- **Default Actions**: Configurable default allow/deny behavior

### Rate Limiting
- **Per-Client Limits**: Token bucket algorithm for per-client rate limiting
- **Global Limits**: System-wide rate limiting protection
- **Connection Limiting**: Maximum concurrent connections per IP
- **Burst Protection**: Configurable burst size and window

### DDoS Protection
- **Anomaly Detection**: Statistical analysis of traffic patterns
- **Automatic Blocking**: IP blocking for detected attacks
- **Connection Rate Limiting**: Per-IP connection limits
- **Request Throttling**: Automatic throttling of suspicious traffic

### Configuration Examples

Security configuration examples are available in multiple formats:

```bash
# Authentication examples
config/examples/security/authentication.conf    # INI format
config/examples/security/authentication.json    # JSON format
config/examples/security/authentication.yaml   # YAML format

# ACL examples
config/examples/security/acl.conf
config/examples/security/acl.json
config/examples/security/acl.yaml

# Rate limiting and DDoS protection examples
config/examples/security/rate-limiting.conf
config/examples/security/rate-limiting.json
config/examples/security/rate-limiting.yaml
```

### Security Tools

```bash
# Run security scan
make security-scan

# Run static analysis
make analyze

# Check code style
make check-style
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests and quality checks
5. Submit a pull request

### Code Quality

```bash
# Format code
make format

# Run linting
make lint

# Run security scan
make security-scan
```

## License

[Add your license information here]

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for a detailed history of changes.
