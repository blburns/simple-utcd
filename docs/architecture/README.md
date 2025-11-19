# Simple UTC Daemon - Architecture

Simple UTC Daemon uses a multi-layered architecture designed for high performance, security, and scalability.

## Overview

The architecture is built around a core UTC server with multiple security layers, asynchronous I/O processing, and comprehensive monitoring capabilities. The design supports both vertical scaling (within a single instance) and horizontal scaling (across multiple instances).

## Main Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Simple UTC Daemon Architecture                       │
└─────────────────────────────────────────────────────────────────────────────┘

                    ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
                    │   Client 1   │  │   Client 2   │  │   Client N   │
                    │  (UTC Query) │  │  (UTC Query) │  │  (UTC Query) │
                    └──────┬───────┘  └──────┬───────┘  └──────┬───────┘
                           │                  │                  │
                           └──────────────────┼──────────────────┘
                                               │
                    ┌──────────────────────────▼──────────────────────────┐
                    │              Network Layer (Port 37)                │
                    │         IPv4/IPv6, TCP/UDP Connection Pool          │
                    └──────────────────────────┬──────────────────────────┘
                                               │
                    ┌──────────────────────────▼──────────────────────────┐
                    │              Security Layer (v0.3.0)                 │
                    │  ┌──────────────┐  ┌──────────────┐  ┌────────────┐ │
                    │  │  ACL Manager │  │ Rate Limiter │  │ DDoS Prot. │ │
                    │  │  (CIDR/IP)   │  │ (Token Bucket│  │ (Anomaly  │ │
                    │  │              │  │  Per-Client) │  │ Detection) │ │
                    │  └──────┬───────┘  └──────┬───────┘  └─────┬──────┘ │
                    │         │                  │                 │        │
                    │  ┌──────▼──────────────────▼─────────────────▼──────┐ │
                    │  │         Authentication (MD5/SHA-1/SHA-256)        │ │
                    │  │         Session Management & Key Validation       │ │
                    │  └──────────────────────┬───────────────────────────┘ │
                    └─────────────────────────┼─────────────────────────────┘
                                               │
                    ┌──────────────────────────▼──────────────────────────┐
                    │            UTC Server Core                          │
                    │  ┌──────────────────────────────────────────────┐  │
                    │  │         Connection Manager                   │  │
                    │  │    (Connection Pool & Lifecycle)            │  │
                    │  └──────────────┬───────────────────────────────┘  │
                    │                 │                                   │
                    │  ┌──────────────▼───────────────────────────────┐  │
                    │  │      Async I/O Manager (Thread Pool)         │  │
                    │  │  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐   │  │
                    │  │  │Worker│  │Worker│  │Worker│  │Worker│   │  │
                    │  │  │Thread│  │Thread│  │Thread│  │Thread│   │  │
                    │  │  │  1   │  │  2   │  │  3   │  │  N   │   │  │
                    │  │  └──┬───┘  └──┬───┘  └──┬───┘  └──┬───┘   │  │
                    │  └─────┼─────────┼─────────┼─────────┼───────┘  │
                    │        │         │         │         │           │
                    │  ┌─────▼─────────▼─────────▼─────────▼───────┐  │
                    │  │      UTC Packet Handler                   │  │
                    │  │  (Parse, Validate, Create Response)      │  │
                    │  └──────────────────┬────────────────────────┘  │
                    └─────────────────────┼───────────────────────────┘
                                          │
                    ┌─────────────────────▼───────────────────────────┐
                    │         Time Synchronization Layer              │
                    │  ┌──────────────────────────────────────────┐  │
                    │  │    System Clock / Upstream Servers       │  │
                    │  │  time.nist.gov  time.google.com  etc.    │  │
                    │  └──────────────────────────────────────────┘  │
                    └────────────────────────────────────────────────┘

                    ┌────────────────────────────────────────────────┐
                    │         Monitoring & Observability              │
                    │  ┌──────────────┐  ┌──────────────┐           │
                    │  │   Metrics    │  │ Health Check │           │
                    │  │ (Prometheus) │  │  (JSON/HTTP) │           │
                    │  └──────────────┘  └──────────────┘           │
                    │  ┌──────────────┐  ┌──────────────┐           │
                    │  │   Logger     │  │ Error Handler │           │
                    │  │ (Structured) │  │  (Recovery)   │           │
                    │  └──────────────┘  └──────────────┘           │
                    └────────────────────────────────────────────────┘
```

## Horizontal Scaling Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Horizontal Scaling Architecture                      │
└─────────────────────────────────────────────────────────────────────────────┘

    ┌──────────┐      ┌──────────┐      ┌──────────┐      ┌──────────┐
    │  Load    │      │  Load    │      │  Load    │      │  Load    │
    │ Balancer │      │ Balancer │      │ Balancer │      │ Balancer │
    └────┬─────┘      └────┬─────┘      └────┬─────┘      └────┬─────┘
         │                 │                 │                 │
    ┌────▼─────────────────▼─────────────────▼─────────────────▼────┐
    │                    Load Balancer Layer                         │
    │              (Round-Robin / Health-Based)                       │
    └────┬─────────────────┬─────────────────┬─────────────────┬────┘
         │                 │                 │                 │
    ┌────▼─────┐      ┌────▼─────┐      ┌────▼─────┐      ┌────▼─────┐
    │ Simple   │      │ Simple   │      │ Simple   │      │ Simple   │
    │ UTC      │      │ UTC      │      │ UTC      │      │ UTC      │
    │ Daemon   │      │ Daemon   │      │ Daemon   │      │ Daemon   │
    │Instance 1│      │Instance 2│      │Instance 3│      │Instance N│
    │          │      │          │      │          │      │          │
    │ ┌──────┐ │      │ ┌──────┐ │      │ ┌──────┐ │      │ ┌──────┐ │
    │ │Worker│ │      │ │Worker│ │      │ │Worker│ │      │ │Worker│ │
    │ │Pool  │ │      │ │Pool  │ │      │ │Pool  │ │      │ │Pool  │ │
    │ └──────┘ │      │ └──────┘ │      │ └──────┘ │      │ └──────┘ │
    └────┬─────┘      └────┬─────┘      └────┬─────┘      └────┬─────┘
         │                 │                 │                 │
         └─────────────────┼─────────────────┼─────────────────┘
                           │                 │
                    ┌──────▼─────────────────▼──────┐
                    │   Shared State (Optional)      │
                    │  - Metrics Aggregation         │
                    │  - Health Status               │
                    │  - Configuration Sync          │
                    └────────────────────────────────┘
```

## Request Flow

The following diagram shows the step-by-step flow of a request through the system:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Request Flow (Single Instance)                       │
└─────────────────────────────────────────────────────────────────────────────┘

    Client Request
         │
         ▼
    ┌─────────────────┐
    │ 1. Network       │  Accept connection on port 37
    │    Connection   │
    └────────┬────────┘
             │
         ▼
    ┌─────────────────┐
    │ 2. ACL Check     │  ✓ IP allowed? ✓ Network in range?
    │    (v0.3.0)     │
    └────────┬────────┘
             │
         ▼
    ┌─────────────────┐
    │ 3. Rate Limit    │  ✓ Under rate limit? ✓ Burst available?
    │    (v0.3.0)     │
    └────────┬────────┘
             │
         ▼
    ┌─────────────────┐
    │ 4. DDoS Check    │  ✓ Normal traffic? ✓ No anomaly?
    │    (v0.3.0)     │
    └────────┬────────┘
             │
         ▼
    ┌─────────────────┐
    │ 5. Authentication│  ✓ Valid key? ✓ Valid signature?
    │    (v0.3.0)     │
    └────────┬────────┘
             │
         ▼
    ┌─────────────────┐
    │ 6. Connection    │  Assign to worker thread
    │    Assignment    │
    └────────┬────────┘
             │
         ▼
    ┌─────────────────┐
    │ 7. Packet Parse  │  Parse UTC packet, validate format
    │    & Validate    │
    └────────┬────────┘
             │
         ▼
    ┌─────────────────┐
    │ 8. Get UTC Time  │  Query system clock or upstream
    │                  │
    └────────┬────────┘
             │
         ▼
    ┌─────────────────┐
    │ 9. Create        │  Build UTC response packet
    │    Response      │
    └────────┬────────┘
             │
         ▼
    ┌─────────────────┐
    │10. Send Response │  Send to client via connection
    │                  │
    └────────┬────────┘
             │
         ▼
    ┌─────────────────┐
    │11. Metrics       │  Record request, response time, etc.
    │    & Logging     │
    └──────────────────┘
```

## Scaling Characteristics

### Vertical Scaling (Single Instance)

- **Worker Threads**: Configurable (default: 2-4, max: CPU cores)
- **Connection Pool**: Configurable (default: 100, max: 10,000+)
- **Memory**: ~10-50MB base + ~1KB per connection
- **CPU**: Single-threaded accept, multi-threaded processing

### Horizontal Scaling (Multiple Instances)

- **Stateless Design**: Each instance independent
- **Load Balancing**: Round-robin, health-based, or geographic
- **Shared State**: Optional metrics aggregation
- **Failover**: Automatic via health checks

### Performance Metrics

- **Latency**: <1ms (local), <10ms (network)
- **Throughput**: 10,000+ requests/second per instance
- **Connections**: 1,000+ concurrent per instance
- **Memory**: Efficient with connection pooling

### Security Layers (v0.3.0)

- **ACL**: O(1) IP lookup, O(n) CIDR matching (n = rules)
- **Rate Limiting**: O(1) token bucket per client
- **DDoS Protection**: O(1) anomaly detection, O(n) pattern analysis
- **Authentication**: O(1) key lookup, O(1) signature verification

## Architecture Components

### Network Layer
- Handles IPv4/IPv6 connections
- TCP/UDP protocol support
- Connection pooling and lifecycle management
- Port 37 (standard UTC port)

### Security Layer (v0.3.0)
- **ACL Manager**: IP-based and CIDR network access control
- **Rate Limiter**: Token bucket algorithm for per-client and global rate limiting
- **DDoS Protection**: Anomaly detection and automatic IP blocking
- **Authentication**: MD5, SHA-1, and SHA-256 authentication with session management

### UTC Server Core
- **Connection Manager**: Manages connection pool and lifecycle
- **Async I/O Manager**: Thread pool for asynchronous request processing
- **UTC Packet Handler**: Parses, validates, and creates UTC packets

### Time Synchronization Layer
- System clock integration
- Upstream server support (time.nist.gov, time.google.com, etc.)
- Time source selection and failover

### Monitoring & Observability
- **Metrics**: Prometheus-compatible metrics collection
- **Health Checks**: JSON and HTTP health endpoints
- **Logger**: Structured logging with multiple levels
- **Error Handler**: Error recovery and reporting

## Design Principles

1. **Security First**: Multi-layered security with defense in depth
2. **High Performance**: Asynchronous I/O with thread pool processing
3. **Scalability**: Stateless design enabling horizontal scaling
4. **Observability**: Comprehensive metrics and health monitoring
5. **Reliability**: Error handling and graceful degradation

## Related Documentation

- [Configuration Guide](../configuration/README.md) - Configuration options
- [Deployment Guide](../deployment/README.md) - Deployment strategies
- [Security Examples](../../config/examples/security/README.md) - Security configuration examples

---

*Last updated: January 2024*

