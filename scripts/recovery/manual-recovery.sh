#!/bin/bash
#
# Manual Recovery Script for Simple UTC Daemon
# Interactive script for manual recovery procedures
#
# Copyright 2024 SimpleDaemons
# Licensed under the Apache License, Version 2.0

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SERVICE_NAME="simple-utcd"
CONFIG_FILE="${CONFIG_FILE:-/etc/simple-utcd/simple-utcd.conf}"
BACKUP_DIR="${BACKUP_DIR:-/var/backups/simple-utcd}"
STATE_DIR="${STATE_DIR:-/var/lib/simple-utcd}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_section() {
    echo -e "\n${YELLOW}>>> $1${NC}"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

prompt_yes_no() {
    local prompt="$1"
    local default="${2:-n}"
    local response
    
    if [ "$default" = "y" ]; then
        read -p "$prompt [Y/n]: " response
        response="${response:-y}"
    else
        read -p "$prompt [y/N]: " response
        response="${response:-n}"
    fi
    
    case "$response" in
        [yY]|[yY][eE][sS])
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

# List available backups
list_backups() {
    print_section "Available Backups"
    
    if [ ! -d "$BACKUP_DIR" ]; then
        print_error "Backup directory not found: $BACKUP_DIR"
        return 1
    fi
    
    echo "Configuration Backups:"
    find "$BACKUP_DIR" -name "config_*.config" -type f -printf "  %T+  %p\n" 2>/dev/null | sort -r | head -10
    
    echo -e "\nState Backups:"
    find "$BACKUP_DIR" -name "state_*.state" -type f -printf "  %T+  %p\n" 2>/dev/null | sort -r | head -10
    
    echo -e "\nMetrics Backups:"
    find "$BACKUP_DIR" -name "metrics_*.metrics" -type f -printf "  %T+  %p\n" 2>/dev/null | sort -r | head -10
}

# Restore configuration
restore_config() {
    print_section "Restore Configuration"
    
    list_backups
    
    echo ""
    read -p "Enter backup file path (or press Enter to use latest): " backup_file
    
    if [ -z "$backup_file" ]; then
        backup_file=$(find "$BACKUP_DIR" -name "config_*.config" -type f -printf '%T@ %p\n' 2>/dev/null | sort -n | tail -1 | cut -d' ' -f2-)
    fi
    
    if [ -z "$backup_file" ] || [ ! -f "$backup_file" ]; then
        print_error "Backup file not found: $backup_file"
        return 1
    fi
    
    print_info "Restoring from: $backup_file"
    
    if prompt_yes_no "Restore configuration?"; then
        cp "$backup_file" "$CONFIG_FILE" || {
            print_error "Failed to restore configuration"
            return 1
        }
        print_success "Configuration restored"
        return 0
    fi
    
    return 1
}

# Restore state
restore_state() {
    print_section "Restore State"
    
    list_backups
    
    echo ""
    read -p "Enter state backup file path (or press Enter to use latest): " backup_file
    
    if [ -z "$backup_file" ]; then
        backup_file=$(find "$BACKUP_DIR" -name "state_*.state" -type f -printf '%T@ %p\n' 2>/dev/null | sort -n | tail -1 | cut -d' ' -f2-)
    fi
    
    if [ -z "$backup_file" ] || [ ! -f "$backup_file" ]; then
        print_error "State backup file not found"
        return 1
    fi
    
    print_info "Restoring state from: $backup_file"
    
    if prompt_yes_no "Restore state?"; then
        mkdir -p "$STATE_DIR"
        cp "$backup_file" "$STATE_DIR/state.restored" || {
            print_error "Failed to restore state"
            return 1
        }
        print_success "State restored to $STATE_DIR/state.restored"
        return 0
    fi
    
    return 1
}

# Service management
manage_service() {
    print_section "Service Management"
    
    echo "1. Start service"
    echo "2. Stop service"
    echo "3. Restart service"
    echo "4. Check status"
    echo ""
    read -p "Select option [1-4]: " option
    
    case "$option" in
        1)
            if command -v systemctl > /dev/null 2>&1; then
                systemctl start "$SERVICE_NAME"
            else
                /etc/init.d/"$SERVICE_NAME" start
            fi
            print_success "Service started"
            ;;
        2)
            if command -v systemctl > /dev/null 2>&1; then
                systemctl stop "$SERVICE_NAME"
            else
                /etc/init.d/"$SERVICE_NAME" stop
            fi
            print_success "Service stopped"
            ;;
        3)
            if command -v systemctl > /dev/null 2>&1; then
                systemctl restart "$SERVICE_NAME"
            else
                /etc/init.d/"$SERVICE_NAME" restart
            fi
            print_success "Service restarted"
            ;;
        4)
            if command -v systemctl > /dev/null 2>&1; then
                systemctl status "$SERVICE_NAME"
            else
                /etc/init.d/"$SERVICE_NAME" status
            fi
            ;;
        *)
            print_error "Invalid option"
            ;;
    esac
}

# View logs
view_logs() {
    print_section "View Logs"
    
    echo "1. Service logs (systemd)"
    echo "2. Application logs"
    echo "3. Recovery logs"
    echo ""
    read -p "Select option [1-3]: " option
    
    case "$option" in
        1)
            if command -v journalctl > /dev/null 2>&1; then
                journalctl -u "$SERVICE_NAME" -n 50 --no-pager
            else
                print_error "journalctl not available"
            fi
            ;;
        2)
            local log_file="/var/log/simple-utcd/simple-utcd.log"
            if [ -f "$log_file" ]; then
                tail -50 "$log_file"
            else
                print_error "Log file not found: $log_file"
            fi
            ;;
        3)
            local log_file="/var/log/simple-utcd/recovery.log"
            if [ -f "$log_file" ]; then
                tail -50 "$log_file"
            else
                print_error "Recovery log not found: $log_file"
            fi
            ;;
        *)
            print_error "Invalid option"
            ;;
    esac
}

# Diagnostic information
run_diagnostics() {
    print_section "Diagnostics"
    
    echo "Service Status:"
    if command -v systemctl > /dev/null 2>&1; then
        systemctl status "$SERVICE_NAME" --no-pager -l || true
    else
        /etc/init.d/"$SERVICE_NAME" status || true
    fi
    
    echo -e "\nConfiguration:"
    if [ -f "$CONFIG_FILE" ]; then
        print_success "Config file exists: $CONFIG_FILE"
        ls -lh "$CONFIG_FILE"
    else
        print_error "Config file not found: $CONFIG_FILE"
    fi
    
    echo -e "\nBackups:"
    if [ -d "$BACKUP_DIR" ]; then
        print_success "Backup directory exists: $BACKUP_DIR"
        local backup_count=$(find "$BACKUP_DIR" -type f | wc -l)
        echo "  Total backups: $backup_count"
    else
        print_error "Backup directory not found: $BACKUP_DIR"
    fi
    
    echo -e "\nDisk Space:"
    df -h "$(dirname "$CONFIG_FILE")" 2>/dev/null || true
    
    echo -e "\nMemory:"
    free -h || true
}

# Main menu
main_menu() {
    while true; do
        print_header "Simple UTC Daemon - Manual Recovery"
        
        echo "1. Restore Configuration"
        echo "2. Restore State"
        echo "3. Manage Service"
        echo "4. View Logs"
        echo "5. Run Diagnostics"
        echo "6. List Backups"
        echo "7. Exit"
        echo ""
        read -p "Select option [1-7]: " option
        
        case "$option" in
            1)
                restore_config
                ;;
            2)
                restore_state
                ;;
            3)
                manage_service
                ;;
            4)
                view_logs
                ;;
            5)
                run_diagnostics
                ;;
            6)
                list_backups
                ;;
            7)
                echo "Exiting..."
                exit 0
                ;;
            *)
                print_error "Invalid option"
                ;;
        esac
        
        echo ""
        read -p "Press Enter to continue..."
    done
}

# Run main menu
main_menu

