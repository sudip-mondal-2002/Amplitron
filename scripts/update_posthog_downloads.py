#!/usr/bin/env python3
"""
Update PostHog insight with GitHub release download counts.

This script fetches download counts from GitHub releases and updates
a PostHog insight with the latest data.

Required environment variable:
  POSTHOG_API_KEY - PostHog personal API key
"""

import json
import os
import sys
import urllib.request as u
from urllib.error import HTTPError, URLError


def fetch_github_releases():
    """Fetch releases from GitHub API."""
    print("📥 Fetching releases from GitHub...")
    try:
        url = "https://api.github.com/repos/sudip-mondal-2002/Amplitron/releases"
        req = u.Request(url, headers={"User-Agent": "Amplitron-CI"})
        response = u.urlopen(req)
        releases = json.loads(response.read())
        print(f"   ✓ Fetched {len(releases)} releases")
        return releases
    except (HTTPError, URLError) as e:
        print(f"   ✗ Failed to fetch releases: {e}")
        sys.exit(1)


def calculate_downloads(releases):
    """Calculate download counts by OS."""
    linux_count = sum(
        a["download_count"]
        for release in releases
        for a in release.get("assets", [])
        if "Linux" in a["name"]
    )
    macos_count = sum(
        a["download_count"]
        for release in releases
        for a in release.get("assets", [])
        if "macOS" in a["name"]
    )
    windows_count = sum(
        a["download_count"]
        for release in releases
        for a in release.get("assets", [])
        if "Windows" in a["name"]
    )

    print(f"📊 Download counts:")
    print(f"   Linux:   {linux_count}")
    print(f"   macOS:   {macos_count}")
    print(f"   Windows: {windows_count}")

    return linux_count, macos_count, windows_count


def update_posthog_insight(linux, macos, windows):
    """Update PostHog insight with download counts."""
    api_key = os.environ.get("POSTHOG_API_KEY")
    if not api_key:
        print("   ✗ Error: POSTHOG_API_KEY environment variable not set")
        print("   Please add POSTHOG_API_KEY to GitHub repository secrets:")
        print("   Settings → Secrets and variables → Actions → New repository secret")
        sys.exit(1)

    print("📤 Updating PostHog insight...")

    # Build HogQL query
    sql = (
        "SELECT 'Linux' AS operating_system, {} AS download_count "
        "UNION ALL "
        "SELECT 'macOS' AS operating_system, {} AS download_count "
        "UNION ALL "
        "SELECT 'Windows' AS operating_system, {} AS download_count"
    ).format(linux, macos, windows)

    payload = {
        "query": {
            "kind": "DataVisualizationNode",
            "source": {
                "kind": "HogQLQuery",
                "query": sql
            }
        }
    }

    try:
        url = "https://us.posthog.com/api/projects/355360/insights/7587728/"
        data = json.dumps(payload).encode("utf-8")
        headers = {
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
            "User-Agent": "Amplitron-CI"
        }
        req = u.Request(url, data=data, headers=headers, method="PATCH")
        response = u.urlopen(req)
        result = json.loads(response.read())
        insight_name = result.get("name", "Unknown insight")
        print(f"   ✓ Successfully updated: {insight_name}")
        return True

    except HTTPError as e:
        error_msg = e.read().decode("utf-8")
        print(f"   ✗ HTTP {e.code} Error: {e.reason}")
        print(f"   Response: {error_msg}")
        if e.code == 401:
            print("   → Check that POSTHOG_API_KEY is correct")
        elif e.code == 404:
            print("   → Check project ID (355360) and insight ID (7587728)")
        sys.exit(1)

    except URLError as e:
        print(f"   ✗ Network Error: {e.reason}")
        sys.exit(1)

    except Exception as e:
        print(f"   ✗ Unexpected error: {e}")
        sys.exit(1)


def main():
    """Main entry point."""
    print("🚀 Amplitron PostHog Downloads Updater\n")

    releases = fetch_github_releases()
    linux, macos, windows = calculate_downloads(releases)
    update_posthog_insight(linux, macos, windows)

    print("\n✅ Success! PostHog insight updated.\n")


if __name__ == "__main__":
    main()
