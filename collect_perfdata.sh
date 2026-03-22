#!/bin/bash
# Compatibility wrapper for the canonical module three collector.

set -e

branch=$(git branch --show-current)
exec bash webui/collect_data.sh "$branch"
