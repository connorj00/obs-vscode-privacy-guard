// Shared transport details for the versioned line-based named-pipe protocol.
export const PIPE_PATH = String.raw`\\.\pipe\obs-vscode-privacy-guard-v1`;
export const STATE_REFRESH_INTERVAL_MS = 500;

export type PrivacyStatus = "safe" | "sensitive";

// Frame builders keep all wire-format serialization in one place.
export function helloFrame(clientId: string): string {
	return `PG/1 HELLO ${clientId}\n`;
}

export function stateFrame(sequence: number, status: PrivacyStatus): string {
	return `PG/1 STATE ${sequence} ${status.toUpperCase()}\n`;
}

export function goodbyeFrame(): string {
	return "PG/1 GOODBYE\n";
}
