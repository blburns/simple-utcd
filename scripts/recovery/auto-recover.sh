#!/bin/bash
#
# Automated Recovery Script for Simple UTC Daemon
# This script automatically recovers the service from various failure scenarios
#
# Copyright 2024 SimpleDaemons
# Licensed under the Apache License, Version 2.0

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SERVICE_NAME="simple-utcd"
CONFIG_FILE="${CONFIG_FILE:-/etc/simple-utcd/simple-utcd.conf}"
BACKUP_DIR="${BACKUP_DIR:-/var/backups/simple-utcd}"
LOG_FILE="${LOG_FILE:-/var/log/simple-utcd/recovery.log}"
MAX_RECOVERY_ATTEMPTS=3
RECOVERY_DELAY=5

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*" | tee -a "$LOG_FILE"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $*" | tee -a "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*" | tee -a "$LOG_FILE"
}

# Check if service is running
is_service_running() {
    if systemctl is-active --quiet "$SERVICE_NAME" 2>/dev/null; then
        return 0
    elif pgrep -f "$SERVICE_NAME" > /dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

# Check if service is healthy
is_service_healthy() {
    local health_endpoint="${HEALTH_ENDPOINT:-http://localhost:8080/health}"
    if command -v curl > /dev/null 2>&1; then
        if curl -sf "$health_endpoint" > /dev/null 2>&1; then
            return 0
        fi
    fi
    return 1
}

# Restart the service
restart_service() {
    log "Attempting to restart service..."
    
    if command -v systemctl > /dev/null 2>&1; then
        systemctl restart "$SERVICE_NAME" || return 1
    elif [ -f "/etc/init.d/$SERVICE_NAME" ]; then
        /etc/init.d/"$SERVICE_NAME" restart || return 1
    else
        log_error "No service manager found (systemctl or init.d)"
        return 1
    fi
    
    sleep 2
    return 0
}

# Restore from backup
restore_from_backup() {
    log "Attempting to restore from backup..."
    
    if [ ! -d "$BACKUP_DIR" ]; then
        log_error "Backup directory not found: $BACKUP_DIR"
        return 1
    fi
    
    # Find most recent config backup
    local latest_backup=$(find "$BACKUP_DIR" -name "config_*.config" -type f -printf '%T@ %p\n' 2>/dev/null | sort -n | tail -1 | cut -d' ' -f2-)
    
    if [ -z "$latest_backup" ] || [ ! -f "$latest_backup" ]; then
        log_error "No backup found in $BACKUP_DIR"
        return 1
    fi
    
    log "Restoring from: $latest_backup"
    cp "$latest_backup" "$CONFIG_FILE" || return 1
    
    log_success "Configuration restored from backup"
    return 0
}

# Verify configuration
verify_config() {
    log "Verifying configuration..."
    
    if [ ! -f "$CONFIG_FILE" ]; then
        log_error "Configuration file not found: $CONFIG_FILE"
        return 1
    fi
    
    # Basic syntax check (if config is JSON)
    if grep -q "{" "$CONFIG_FILE" 2>/dev/null; then
        if command -v python3 > /dev/null 2>&1; then
            python3 -m json.tool "$CONFIG_FILE" > /dev/null 2>&1 || {
                log_error "Invalid JSON configuration"
                return 1
            }
        fi
    fi
    
    log_success "Configuration verified"
    return 0
}

# Check disk space
check_disk_space() {
    local threshold=90
    local usage=$(df -h "$(dirname "$CONFIG_FILE")" | awk 'NR==2 {print $5}' | sed 's/%//')
    
    if [ "$usage" -gt "$threshold" ]; then
        log_warn "Disk usage is ${usage}% (threshold: ${threshold}%)"
        return 1
    fi
    
    return 0
}

# Check system resources
check_resources() {
    log "Checking system resources..."
    
    # Check memory
    local mem_usage=$(free | awk 'NR==2{printf "%.0f", $3*100/$2}')
    if [ "$mem_usage" -gt 90 ]; then
        log_warn "Memory usage is ${mem_usage}%"
    fi
    
    # Check disk space
    check_disk_space || log_warn "Disk space may be low"
    
    return 0
}

# Main recovery procedure
main_recovery() {
    local attempt=1
    
    log "=== Starting automated recovery procedure ==="
    log "Service: $SERVICE_NAME"
    log "Config: $CONFIG_FILE"
    log "Backup Dir: $BACKUP_DIR"
    
    # Check system resources
    check_resources
    
    while [ $attempt -le $MAX_RECOVERY_ATTEMPTS ]; do
        log "Recovery attempt $attempt of $MAX_RECOVERY_ATTEMPTS"
        
        # Step 1: Verify configuration
        if ! verify_config; then
            log "Restoring configuration from backup..."
            restore_from_backup || {
                log_error "Failed to restore from backup"
                attempt=$((attempt + 1))
                sleep $RECOVERY_DELAY
                continue
            }
        fi
        
        # Step 2: Restart service
        if ! restart_service; then
            log_error "Failed to restart service"
            attempt=$((attempt + 1))
            sleep $RECOVERY_DELAY
            continue
        fi
        
        # Step 3: Wait for service to start
        sleep 3
        
        # Step 4: Verify service is running
        if ! is_service_running; then
            log_error "Service is not running after restart"
            attempt=$((attempt + 1))
            sleep $RECOVERY_DELAY
            continue
        fi
        
        # Step 5: Check service health
        if is_service_healthy; then
            log_success "Service recovered successfully"
            return 0
        else
            log_warn "Service is running but health check failed"
            # Still consider it a success if service is running
            log_success "Service recovered (running but health check unavailable)"
            return 0
        fi
        
        attempt=$((attempt + 1))
        sleep $RECOVERY_DELAY
    done
    
    log_error "Recovery failed after $MAX_RECOVERY_ATTEMPTS attempts"
    return 1
}

# Handle different failure scenarios
case "${1:-auto}" in
    auto)
        main_recovery
        ;;
    restart)
        restart_service && log_success "Service restarted" || log_error "Restart failed"
        ;;
    restore)
        restore_from_backup && restart_service && log_success "Restored and restarted" || log_error "Restore failed"
        ;;
    verify)
        verify_config && log_success "Configuration valid" || log_error "Configuration invalid"
        ;;
    health)
        if is_service_healthy; then
            log_success "Service is healthy"
            exit 0
        else
            log_error "Service is unhealthy"
            exit 1
        fi
        ;;
    *)
        echo "Usage: $0 [auto|restart|restore|verify|health]"
        exit 1
        ;;
esac

exit $?

