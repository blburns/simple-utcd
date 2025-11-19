# Recovery Scripts

This directory contains recovery scripts for the Simple UTC Daemon.

## Scripts

### `auto-recover.sh`

Automated recovery script that automatically detects and recovers from common failure scenarios.

**Usage:**
```bash
# Full automated recovery
sudo ./auto-recover.sh auto

# Just restart service
sudo ./auto-recover.sh restart

# Restore from backup and restart
sudo ./auto-recover.sh restore

# Verify configuration
sudo ./auto-recover.sh verify

# Check service health
sudo ./auto-recover.sh health
```

**Configuration:**
Set environment variables to customize behavior:
- `CONFIG_FILE`: Configuration file path (default: `/etc/simple-utcd/simple-utcd.conf`)
- `BACKUP_DIR`: Backup directory (default: `/var/backups/simple-utcd`)
- `LOG_FILE`: Recovery log file (default: `/var/log/simple-utcd/recovery.log`)
- `HEALTH_ENDPOINT`: Health check endpoint (default: `http://localhost:8080/health`)

### `manual-recovery.sh`

Interactive manual recovery script for operator-guided recovery operations.

**Usage:**
```bash
sudo ./manual-recovery.sh
```

**Features:**
- Restore configuration from backups
- Restore state from backups
- Service management (start/stop/restart/status)
- View logs (service, application, recovery)
- Run diagnostics
- List available backups

## Documentation

For detailed disaster recovery procedures, see:
- [Disaster Recovery Guide](../../docs/recovery/disaster-recovery.md)

## Requirements

- Bash 4.0+
- Systemd or init.d service management
- Root/sudo access
- curl (optional, for health checks)

## Examples

### Automated Recovery
```bash
# Run full automated recovery
sudo ./auto-recover.sh auto

# Check recovery status
tail -f /var/log/simple-utcd/recovery.log
```

### Manual Recovery
```bash
# Start interactive recovery
sudo ./manual-recovery.sh

# Follow the menu prompts to:
# 1. Restore configuration
# 2. Restore state
# 3. Manage service
# 4. View logs
# 5. Run diagnostics
```

### Scheduled Recovery Checks
```bash
# Add to crontab for periodic health checks
*/5 * * * * /path/to/scripts/recovery/auto-recover.sh health
```

---

*Last updated: January 2024*

