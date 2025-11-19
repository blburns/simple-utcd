# Disaster Recovery Guide

This document provides comprehensive procedures for recovering the Simple UTC Daemon from various disaster scenarios.

## Table of Contents

1. [Overview](#overview)
2. [Recovery Procedures](#recovery-procedures)
3. [Automated Recovery](#automated-recovery)
4. [Manual Recovery](#manual-recovery)
5. [Disaster Scenarios](#disaster-scenarios)
6. [Prevention](#prevention)
7. [Testing Recovery Procedures](#testing-recovery-procedures)

## Overview

The Simple UTC Daemon includes comprehensive recovery mechanisms to handle various failure scenarios:

- **Automated Recovery**: Scripts that automatically detect and recover from common failures
- **Manual Recovery**: Interactive tools for operator-guided recovery
- **Backup and Restore**: Configuration, state, and metrics backup/restore capabilities
- **Health Monitoring**: Continuous health checks and automatic failover

## Recovery Procedures

### Quick Recovery Checklist

1. **Assess the Situation**
   - Check service status
   - Review logs
   - Identify failure type

2. **Attempt Automated Recovery**
   ```bash
   sudo scripts/recovery/auto-recover.sh
   ```

3. **If Automated Recovery Fails**
   ```bash
   sudo scripts/recovery/manual-recovery.sh
   ```

4. **Verify Recovery**
   - Check service status
   - Verify health endpoints
   - Test UTC time queries

## Automated Recovery

The automated recovery script (`scripts/recovery/auto-recover.sh`) performs the following steps:

### Recovery Steps

1. **System Resource Check**
   - Verifies disk space availability
   - Checks memory usage
   - Validates system health

2. **Configuration Verification**
   - Validates configuration file syntax
   - Restores from backup if invalid
   - Verifies configuration integrity

3. **Service Restart**
   - Stops the service gracefully
   - Restarts the service
   - Waits for service initialization

4. **Health Verification**
   - Checks if service is running
   - Verifies health endpoints
   - Confirms service functionality

### Usage

```bash
# Full automated recovery
sudo scripts/recovery/auto-recover.sh auto

# Just restart service
sudo scripts/recovery/auto-recover.sh restart

# Restore from backup and restart
sudo scripts/recovery/auto-recover.sh restore

# Verify configuration
sudo scripts/recovery/auto-recover.sh verify

# Check service health
sudo scripts/recovery/auto-recover.sh health
```

### Configuration

Set environment variables to customize behavior:

```bash
export CONFIG_FILE=/etc/simple-utcd/simple-utcd.conf
export BACKUP_DIR=/var/backups/simple-utcd
export LOG_FILE=/var/log/simple-utcd/recovery.log
export HEALTH_ENDPOINT=http://localhost:8080/health
```

## Manual Recovery

The manual recovery script (`scripts/recovery/manual-recovery.sh`) provides an interactive interface for recovery operations.

### Features

- **Configuration Restore**: Select and restore from available backups
- **State Restore**: Restore runtime state from backups
- **Service Management**: Start, stop, restart, and check service status
- **Log Viewing**: Access service, application, and recovery logs
- **Diagnostics**: Run comprehensive system diagnostics
- **Backup Listing**: View available backups with timestamps

### Usage

```bash
sudo scripts/recovery/manual-recovery.sh
```

The script provides an interactive menu for all recovery operations.

## Disaster Scenarios

### Scenario 1: Service Crash

**Symptoms:**
- Service is not running
- No response to UTC queries
- Process not found

**Recovery Steps:**

1. Check service status:
   ```bash
   systemctl status simple-utcd
   ```

2. Run automated recovery:
   ```bash
   sudo scripts/recovery/auto-recover.sh
   ```

3. If automated recovery fails, check logs:
   ```bash
   journalctl -u simple-utcd -n 100
   ```

4. Restore from backup if needed:
   ```bash
   sudo scripts/recovery/manual-recovery.sh
   # Select option 1: Restore Configuration
   ```

### Scenario 2: Configuration Corruption

**Symptoms:**
- Service fails to start
- Configuration validation errors
- Invalid configuration syntax

**Recovery Steps:**

1. Verify configuration:
   ```bash
   sudo scripts/recovery/auto-recover.sh verify
   ```

2. Restore from backup:
   ```bash
   sudo scripts/recovery/auto-recover.sh restore
   ```

3. Or use manual recovery:
   ```bash
   sudo scripts/recovery/manual-recovery.sh
   # Select option 1: Restore Configuration
   ```

### Scenario 3: Disk Space Exhaustion

**Symptoms:**
- Service cannot write logs
- Backup operations fail
- Disk usage > 90%

**Recovery Steps:**

1. Free disk space:
   ```bash
   # Clean old backups
   find /var/backups/simple-utcd -type f -mtime +30 -delete
   
   # Clean old logs
   find /var/log/simple-utcd -type f -mtime +7 -delete
   ```

2. Restart service:
   ```bash
   sudo scripts/recovery/auto-recover.sh restart
   ```

### Scenario 4: Network Connectivity Issues

**Symptoms:**
- Cannot connect to upstream servers
- Health checks fail
- Time synchronization errors

**Recovery Steps:**

1. Check network connectivity:
   ```bash
   ping -c 3 time.nist.gov
   ```

2. Verify DNS resolution:
   ```bash
   nslookup time.nist.gov
   ```

3. Check firewall rules:
   ```bash
   iptables -L -n | grep 37
   ```

4. Restart service:
   ```bash
   sudo scripts/recovery/auto-recover.sh restart
   ```

### Scenario 5: Complete System Failure

**Symptoms:**
- Server is down
- All services unavailable
- Need to restore from backup

**Recovery Steps:**

1. **Restore System**
   - Rebuild server from backup
   - Restore operating system
   - Install Simple UTC Daemon

2. **Restore Configuration**
   ```bash
   # Copy backup to config location
   cp /backup/location/config_backup.conf /etc/simple-utcd/simple-utcd.conf
   ```

3. **Restore State** (if available)
   ```bash
   sudo scripts/recovery/manual-recovery.sh
   # Select option 2: Restore State
   ```

4. **Start Service**
   ```bash
   systemctl start simple-utcd
   systemctl enable simple-utcd
   ```

5. **Verify Recovery**
   ```bash
   systemctl status simple-utcd
   scripts/recovery/auto-recover.sh health
   ```

## Prevention

### Best Practices

1. **Regular Backups**
   - Enable automatic backups
   - Verify backup integrity regularly
   - Store backups off-site

2. **Monitoring**
   - Set up health check monitoring
   - Configure alerting for failures
   - Monitor disk space and resources

3. **Configuration Management**
   - Use version control for configurations
   - Test configuration changes in staging
   - Document all configuration changes

4. **Resource Management**
   - Monitor disk space usage
   - Set up log rotation
   - Clean old backups automatically

5. **Testing**
   - Regularly test recovery procedures
   - Perform disaster recovery drills
   - Document lessons learned

### Backup Configuration

Configure automatic backups in your configuration:

```ini
[backup]
enable_auto_backup = true
backup_directory = /var/backups/simple-utcd
max_backups = 10
retention_days = 30
```

### Health Monitoring

Set up external monitoring:

```bash
# Health check endpoint
curl http://localhost:8080/health

# Metrics endpoint
curl http://localhost:8080/metrics
```

## Testing Recovery Procedures

### Regular Testing Schedule

- **Weekly**: Test automated recovery script
- **Monthly**: Full disaster recovery drill
- **Quarterly**: Complete system restore test

### Test Scenarios

1. **Service Crash Test**
   ```bash
   # Simulate crash
   killall simple-utcd
   
   # Test recovery
   sudo scripts/recovery/auto-recover.sh
   ```

2. **Configuration Corruption Test**
   ```bash
   # Corrupt configuration
   echo "invalid config" >> /etc/simple-utcd/simple-utcd.conf
   
   # Test recovery
   sudo scripts/recovery/auto-recover.sh restore
   ```

3. **Backup Restore Test**
   ```bash
   # Use manual recovery to test restore
   sudo scripts/recovery/manual-recovery.sh
   ```

### Recovery Time Objectives (RTO)

- **Automated Recovery**: < 5 minutes
- **Manual Recovery**: < 15 minutes
- **Full System Restore**: < 1 hour

### Recovery Point Objectives (RPO)

- **Configuration**: < 1 hour (automatic backups)
- **State**: < 1 hour (automatic backups)
- **Metrics**: < 24 hours (periodic backups)

## Emergency Contacts

- **System Administrator**: [Contact Info]
- **On-Call Engineer**: [Contact Info]
- **Escalation**: [Contact Info]

## Additional Resources

- [Configuration Guide](../configuration/README.md)
- [Deployment Guide](../deployment/README.md)
- [Troubleshooting Guide](../troubleshooting/README.md)
- [Backup and Restore API Documentation](../../include/simple_utcd/backup_restore.hpp)

---

*Last updated: January 2024*

