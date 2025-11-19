# simple-utcd Deployment

This directory contains deployment configurations and examples for simple-utcd.

## Directory Structure

```
deployment/
├── systemd/                    # Linux systemd service files
│   └── simple-utcd.service
├── launchd/                    # macOS launchd service files
│   └── com.simple-utcd.simple-utcd.plist
├── logrotate.d/                # Linux log rotation configuration
│   └── simple-utcd
├── windows/                    # Windows service management
│   └── simple-utcd.service.bat
└── examples/                   # Deployment examples
    └── docker/                 # Docker deployment examples
        ├── docker-compose.yml
        └── README.md
```

## Platform-Specific Deployment

### Linux (systemd)

1. **Install the service file:**
   ```bash
   sudo cp deployment/systemd/simple-utcd.service /etc/systemd/system/
   sudo systemctl daemon-reload
   ```

2. **Create user and group:**
   ```bash
   sudo useradd --system --no-create-home --shell /bin/false simple-utcd
   ```

3. **Enable and start the service:**
   ```bash
   sudo systemctl enable simple-utcd
   sudo systemctl start simple-utcd
   ```

4. **Check status:**
   ```bash
   sudo systemctl status simple-utcd
   sudo journalctl -u simple-utcd -f
   ```

### macOS (launchd)

1. **Install the plist file:**
   ```bash
   sudo cp deployment/launchd/com.simple-utcd.simple-utcd.plist /Library/LaunchDaemons/
   sudo chown root:wheel /Library/LaunchDaemons/com.simple-utcd.simple-utcd.plist
   ```

2. **Load and start the service:**
   ```bash
   sudo launchctl load /Library/LaunchDaemons/com.simple-utcd.simple-utcd.plist
   sudo launchctl start com.simple-utcd.simple-utcd
   ```

3. **Check status:**
   ```bash
   sudo launchctl list | grep simple-utcd
   tail -f /var/log/simple-utcd.log
   ```

### Windows

1. **Run as Administrator:**
   ```cmd
   # Install service
   deployment\windows\simple-utcd.service.bat install
   
   # Start service
   deployment\windows\simple-utcd.service.bat start
   
   # Check status
   deployment\windows\simple-utcd.service.bat status
   ```

2. **Service management:**
   ```cmd
   # Stop service
   deployment\windows\simple-utcd.service.bat stop
   
   # Restart service
   deployment\windows\simple-utcd.service.bat restart
   
   # Uninstall service
   deployment\windows\simple-utcd.service.bat uninstall
   ```

## Log Rotation (Linux)

1. **Install logrotate configuration:**
   ```bash
   sudo cp deployment/logrotate.d/simple-utcd /etc/logrotate.d/
   ```

2. **Test logrotate configuration:**
   ```bash
   sudo logrotate -d /etc/logrotate.d/simple-utcd
   ```

3. **Force log rotation:**
   ```bash
   sudo logrotate -f /etc/logrotate.d/simple-utcd
   ```

## Docker Deployment

See [examples/docker/README.md](examples/docker/README.md) for detailed Docker deployment instructions.

### Quick Start

```bash
# Build and run with Docker Compose
cd deployment/examples/docker
docker-compose up -d

# Check status
docker-compose ps
docker-compose logs simple-utcd
```

## Configuration

### Service Configuration

Each platform has specific configuration requirements:

- **Linux**: Edit `/etc/systemd/system/simple-utcd.service`
- **macOS**: Edit `/Library/LaunchDaemons/com.simple-utcd.simple-utcd.plist`
- **Windows**: Edit the service binary path in the batch file

### Application Configuration

Place your application configuration in:
- **Linux/macOS**: `/etc/simple-utcd/simple-utcd.conf`
- **Windows**: `%PROGRAMFILES%\simple-utcd\simple-utcd.conf`

## Security Considerations

### User and Permissions

1. **Create dedicated user:**
   ```bash
   # Linux
   sudo useradd --system --no-create-home --shell /bin/false simple-utcd
   
   # macOS
   sudo dscl . -create /Users/_simple-utcd UserShell /usr/bin/false
   sudo dscl . -create /Users/_simple-utcd UniqueID 200
   sudo dscl . -create /Users/_simple-utcd PrimaryGroupID 200
   sudo dscl . -create /Groups/_simple-utcd GroupID 200
   ```

2. **Set proper permissions:**
   ```bash
   # Configuration files
   sudo chown root:simple-utcd /etc/simple-utcd/simple-utcd.conf
   sudo chmod 640 /etc/simple-utcd/simple-utcd.conf
   
   # Log files
   sudo chown simple-utcd:simple-utcd /var/log/simple-utcd/
   sudo chmod 755 /var/log/simple-utcd/
   ```

### Firewall Configuration

Configure firewall rules as needed:

```bash
# Linux (ufw)
sudo ufw allow 8080/tcp

# Linux (firewalld)
sudo firewall-cmd --permanent --add-port=8080/tcp
sudo firewall-cmd --reload

# macOS
sudo pfctl -f /etc/pf.conf
```

## Monitoring

### Health Checks

1. **Service status:**
   ```bash
   # Linux
   sudo systemctl is-active simple-utcd
   
   # macOS
   sudo launchctl list | grep simple-utcd
   
   # Windows
   sc query simple-utcd
   ```

2. **Port availability:**
   ```bash
   netstat -tlnp | grep 8080
   ss -tlnp | grep 8080
   ```

3. **Process monitoring:**
   ```bash
   ps aux | grep simple-utcd
   top -p $(pgrep simple-utcd)
   ```

### Log Monitoring

1. **Real-time logs:**
   ```bash
   # Linux
   sudo journalctl -u simple-utcd -f
   
   # macOS
   tail -f /var/log/simple-utcd.log
   
   # Windows
   # Use Event Viewer or PowerShell Get-WinEvent
   ```

2. **Log analysis:**
   ```bash
   # Search for errors
   sudo journalctl -u simple-utcd --since "1 hour ago" | grep -i error
   
   # Count log entries
   sudo journalctl -u simple-utcd --since "1 day ago" | wc -l
   ```

## Troubleshooting

### Common Issues

1. **Service won't start:**
   - Check configuration file syntax
   - Verify user permissions
   - Check port availability
   - Review service logs

2. **Permission denied:**
   - Ensure service user exists
   - Check file permissions
   - Verify directory ownership

3. **Port already in use:**
   - Check what's using the port: `netstat -tlnp | grep 8080`
   - Stop conflicting service or change port

4. **Service stops unexpectedly:**
   - Check application logs
   - Verify resource limits
   - Review system logs

### Debug Mode

Run the service in debug mode for troubleshooting:

```bash
# Linux/macOS
sudo -u simple-utcd /usr/local/bin/simple-utcd --debug

# Windows
simple-utcd.exe --debug
```

### Log Levels

Adjust log level for more verbose output:

```bash
# Set log level in configuration
log_level = debug

# Or via environment variable
export SIMPLE-UTCD_LOG_LEVEL=debug
```

## Backup and Recovery

### Configuration Backup

```bash
# Backup configuration
sudo tar -czf simple-utcd-config-backup-$(date +%Y%m%d).tar.gz /etc/simple-utcd/

# Backup logs
sudo tar -czf simple-utcd-logs-backup-$(date +%Y%m%d).tar.gz /var/log/simple-utcd/
```

### Service Recovery

```bash
# Stop service
sudo systemctl stop simple-utcd

# Restore configuration
sudo tar -xzf simple-utcd-config-backup-YYYYMMDD.tar.gz -C /

# Start service
sudo systemctl start simple-utcd
```

## Updates

### Service Update Process

1. **Stop service:**
   ```bash
   sudo systemctl stop simple-utcd
   ```

2. **Backup current version:**
   ```bash
   sudo cp /usr/local/bin/simple-utcd /usr/local/bin/simple-utcd.backup
   ```

3. **Install new version:**
   ```bash
   sudo cp simple-utcd /usr/local/bin/
   sudo chmod +x /usr/local/bin/simple-utcd
   ```

4. **Start service:**
   ```bash
   sudo systemctl start simple-utcd
   ```

5. **Verify update:**
   ```bash
   sudo systemctl status simple-utcd
   simple-utcd --version
   ```
