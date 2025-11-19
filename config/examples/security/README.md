# Security Configuration Examples

This directory contains configuration examples for Simple UTC Daemon security features (Version 0.3.0).

## Available Examples

### Authentication Examples

Examples demonstrating how to configure authentication with MD5, SHA-1, and SHA-256 algorithms.

**Files:**
- `authentication.conf` - INI format
- `authentication.json` - JSON format
- `authentication.yaml` - YAML format

**Features:**
- Multiple authentication algorithms (MD5, SHA-1, SHA-256)
- Key management and rotation
- Session management with configurable timeouts
- Lockout protection after failed attempts
- Environment variable support for secure key storage

### Access Control List (ACL) Examples

Examples demonstrating IP-based and network-based access control using CIDR notation.

**Files:**
- `acl.conf` - INI format
- `acl.json` - JSON format
- `acl.yaml` - YAML format

**Features:**
- IP-based allow/deny rules
- CIDR network notation support (e.g., `192.168.1.0/24`)
- Rule-based ACL with priorities
- Default action configuration (ALLOW/DENY)
- Multiple network support

### Rate Limiting & DDoS Protection Examples

Examples demonstrating rate limiting and DDoS protection features.

**Files:**
- `rate-limiting.conf` - INI format
- `rate-limiting.json` - JSON format
- `rate-limiting.yaml` - YAML format

**Features:**
- Per-client rate limiting (token bucket algorithm)
- Global rate limiting
- Connection limiting per IP
- DDoS protection with anomaly detection
- Automatic IP blocking
- Burst protection
- Request throttling

## Usage

### Using Configuration Files

1. **Copy the example file** to your configuration directory:
   ```bash
   cp config/examples/security/authentication.conf /etc/simple-utcd/simple-utcd.conf
   ```

2. **Edit the configuration** to match your environment:
   ```bash
   vi /etc/simple-utcd/simple-utcd.conf
   ```

3. **Set environment variables** for sensitive values:
   ```bash
   export SIMPLE_UTCD_AUTH_KEY="your-secret-key-here"
   ```

4. **Start the daemon** with your configuration:
   ```bash
   simple-utcd -c /etc/simple-utcd/simple-utcd.conf
   ```

### Environment Variables

Sensitive configuration values should be set via environment variables:

```bash
# Authentication
export SIMPLE_UTCD_AUTH_KEY="your-authentication-key"
export AUTH_KEY_1="key1-value"
export AUTH_KEY_2="key2-value"

# Other overrides
export SIMPLE_UTCD_LISTEN_PORT=37
export SIMPLE_UTCD_LOG_LEVEL=INFO
```

## Configuration Format Support

Simple UTC Daemon supports three configuration formats:

1. **INI format** (`.conf`) - Traditional, human-readable format
2. **JSON format** (`.json`) - Structured, machine-readable format
3. **YAML format** (`.yaml`) - Human-readable structured format

The format is auto-detected based on file extension.

## Security Best Practices

1. **Never commit secrets** to version control
2. **Use environment variables** for authentication keys
3. **Restrict file permissions** on configuration files:
   ```bash
   chmod 600 /etc/simple-utcd/simple-utcd.conf
   ```
4. **Use ACL rules** to restrict access to trusted networks
5. **Enable rate limiting** to prevent abuse
6. **Monitor logs** for security events
7. **Regularly rotate** authentication keys

## Example: Combining Security Features

You can combine multiple security features in a single configuration:

```ini
# Enable authentication
enable_authentication = true
authentication_algorithm = SHA256

# Configure ACL
allowed_clients = ["192.168.1.0/24"]
denied_clients = ["192.168.1.100"]

# Enable rate limiting
enable_rate_limiting = true
rate_limit_requests_per_second = 100

# Enable DDoS protection
enable_ddos_protection = true
ddos_threshold = 1000
```

## Documentation

For more information, see:
- [Main README](../../../README.md)
- [Roadmap Checklist](../../../ROADMAP_CHECKLIST.md)
- [Development Roadmap](../../../ROADMAP.md)

