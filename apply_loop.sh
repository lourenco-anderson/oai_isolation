#!/bin/bash

# =========================================================
# CONFIGURATION
# =========================================================

# Time between apply and delete
RESOURCE_LIFETIME="3h"

# Delay before next manifest
NEXT_DELAY="10m"

# File containing relative paths
PATHS_FILE="paths.txt"

# Log file
LOG_FILE="kubectl_apply.log"

# Base directory (directory where script is located)
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# =========================================================
# FUNCTIONS
# =========================================================

timestamp() {
    date '+%Y-%m-%d %H:%M:%S'
}

log() {
    local STATUS="$1"
    local MESSAGE="$2"

    echo "[$(timestamp)] [$STATUS] $MESSAGE" >> "$LOG_FILE"
}

run_apply() {
    local RELATIVE_PATH="$1"

    FULL_PATH="$BASE_DIR/$RELATIVE_PATH"

    log "INFO" "Running command: kubectl apply -f $FULL_PATH"

    OUTPUT=$(kubectl apply -f "$FULL_PATH" 2>&1)
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 0 ]; then
        log "SUCCESS" "kubectl apply -f $FULL_PATH"
        log "OUTPUT" "$OUTPUT"
    else
        log "ERROR" "kubectl apply -f $FULL_PATH"
        log "OUTPUT" "$OUTPUT"
    fi
}

run_delete() {
    local RELATIVE_PATH="$1"

    FULL_PATH="$BASE_DIR/$RELATIVE_PATH"

    log "INFO" "Running command: kubectl delete -f $FULL_PATH"

    OUTPUT=$(kubectl delete -f "$FULL_PATH" 2>&1)
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 0 ]; then
        log "SUCCESS" "kubectl delete -f $FULL_PATH"
        log "OUTPUT" "$OUTPUT"
    else
        log "ERROR" "kubectl delete -f $FULL_PATH"
        log "OUTPUT" "$OUTPUT"
    fi
}

# =========================================================
# VALIDATIONS
# =========================================================

if [ ! -f "$PATHS_FILE" ]; then
    echo "Paths file not found: $PATHS_FILE"
    exit 1
fi

log "INFO" "========== SCRIPT STARTED =========="

# =========================================================
# MAIN LOOP
# =========================================================

while IFS= read -r PATH_LINE || [ -n "$PATH_LINE" ]
do
    # Ignore empty lines
    [[ -z "$PATH_LINE" ]] && continue

    # Ignore comments
    [[ "$PATH_LINE" =~ ^# ]] && continue

    echo "Processing: $PATH_LINE"

    log "INFO" "Starting processing for: $PATH_LINE"

    # =====================================================
    # STEP 1 - APPLY
    # =====================================================

    run_apply "$PATH_LINE"

    # =====================================================
    # STEP 2 - WAIT 4 HOURS
    # =====================================================

    log "INFO" "Waiting $RESOURCE_LIFETIME before deleting: $PATH_LINE"
    sleep "$RESOURCE_LIFETIME"

    # =====================================================
    # STEP 3 - DELETE
    # =====================================================

    run_delete "$PATH_LINE"

    # =====================================================
    # STEP 4 - WAIT 10 MIN
    # =====================================================

    log "INFO" "Waiting $NEXT_DELAY before next path"
    sleep "$NEXT_DELAY"

done < "$PATHS_FILE"

log "INFO" "========== SCRIPT FINISHED =========="