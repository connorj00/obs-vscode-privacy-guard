# Privacy Policy

Last updated: 28 August 2026

OBS VS Code Privacy Guard is designed to protect information shown during a
stream without collecting or transmitting that information.

## Information processed locally

The VS Code extension compares the basenames of visible editor files against
the filename rules configured by the user. Matching is performed within the
VS Code extension host. File contents are not read for matching.

The extension sends only the following protocol information to the OBS plugin:

- A randomly generated client identifier for the current extension session.
- Whether the visible editors are currently safe or sensitive.
- Sequence numbers, periodic state refreshes, and connection lifecycle messages.

Filenames, file paths, workspace names, file contents, and configured filename
rules are not sent to OBS.

## Local communication

The VS Code extension and OBS plugin communicate through the local Windows
named pipe `\\.\pipe\obs-vscode-privacy-guard-v1`. The pipe is available only
on the computer running Privacy Guard and is not an internet service.

## Settings and user-selected files

VS Code stores extension settings through its normal settings system. OBS
stores plugin settings in the active OBS scene collection. These settings can
include user-selected image paths and OBS scene names. Custom privacy images
are loaded directly from the path selected by the user and are not uploaded.

## Telemetry and external services

Privacy Guard does not include telemetry, analytics, advertising, crash
reporting, user tracking, or runtime communication with external services.

This program will not transfer any information to other networked systems
unless specifically requested by the user or the person installing or
operating it.

Development and installation tools may download documented build dependencies
from their official sources. Those tools are not used by the installed runtime
to transmit user information.

## Questions and changes

Questions or proposed changes to this policy can be submitted through the
[official GitHub repository](https://github.com/connorj00/obs-vscode-privacy-guard/issues).
Material changes to the project's data handling will be documented here before
they are included in a release.
