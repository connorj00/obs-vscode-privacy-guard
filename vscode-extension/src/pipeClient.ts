import { randomUUID } from "node:crypto";
import { createConnection, type Socket } from "node:net";
import type { LogOutputChannel } from "vscode";

import {
	goodbyeFrame,
	helloFrame,
	PIPE_PATH,
	type PrivacyStatus,
	STATE_REFRESH_INTERVAL_MS,
	stateFrame,
} from "./protocol";

const RECONNECT_INTERVAL_MS = 1_000;

export type ConnectionState = "connected" | "disconnected";

// Owns the resilient VS Code-to-OBS named-pipe connection.
export class PipeClient {
	// Connection identity and the latest state survive automatic reconnects.
	readonly #clientId = randomUUID();
	readonly #log: LogOutputChannel;
	readonly #onConnectionState: (state: ConnectionState) => void;
	#socket: Socket | undefined;
	#stateRefreshTimer: NodeJS.Timeout | undefined;
	#reconnectTimer: NodeJS.Timeout | undefined;
	#sequence = 0;
	#latestStatus: PrivacyStatus = "sensitive";
	#disposed = false;

	public constructor(
		log: LogOutputChannel,
		onConnectionState: (state: ConnectionState) => void,
	) {
		this.#log = log;
		this.#onConnectionState = onConnectionState;
	}

	public connect(): void {
		// Only one socket may be connecting or connected at a time.
		if (this.#disposed || this.#socket !== undefined) {
			return;
		}

		const socket = createConnection(PIPE_PATH);
		this.#socket = socket;

		socket.setEncoding("utf8");
		socket.once("connect", () => this.#handleConnected(socket));
		socket.once("error", (error) => { this.#log.debug(`Named-pipe connection failed: ${error.message}`); });
		socket.once("close", () => this.#handleClosed(socket));
	}

	public publish(status: PrivacyStatus): void {
		// Cache every state so a reconnect immediately receives the latest result.
		this.#latestStatus = status;
		if (this.#socket?.readyState === "open") {
			this.#socket.write(stateFrame(++this.#sequence, status));
		}
	}

	public dispose(): void {
		// A graceful GOODBYE lets OBS remove this client without latching a failure.
		this.#disposed = true;
		this.#clearTimers();

		if (this.#socket?.readyState === "open") {
			this.#socket.end(goodbyeFrame());
		} else {
			this.#socket?.destroy();
		}
		this.#socket = undefined;
	}

	#handleConnected(socket: Socket): void {
		// Ignore connections superseded while their asynchronous event was pending.
		if (socket !== this.#socket || this.#disposed) {
			socket.destroy();
			return;
		}

		this.#log.info("Connected to the OBS Privacy Guard plugin.");
		this.#onConnectionState("connected");
		socket.write(helloFrame(this.#clientId));
		socket.write(stateFrame(++this.#sequence, this.#latestStatus));
		// Periodic explicit states provide liveness and allow OBS to recover safely
		// after Privacy Guard is re-enabled.
		this.#stateRefreshTimer = setInterval(() => {
			if (socket.readyState === "open") {
				socket.write(stateFrame(++this.#sequence, this.#latestStatus));
			}
		}, STATE_REFRESH_INTERVAL_MS);
	}

	#handleClosed(socket: Socket): void {
		// Unexpected disconnects are retried until the extension is disposed.
		if (socket !== this.#socket) {
			return;
		}

		this.#socket = undefined;
		this.#clearStateRefresh();
		this.#onConnectionState("disconnected");

		if (!this.#disposed) {
			this.#reconnectTimer = setTimeout(
				() => this.connect(),
				RECONNECT_INTERVAL_MS,
			);
		}
	}

	#clearStateRefresh(): void {
		// Timer cleanup is kept separate because reconnects retain their own timer.
		if (this.#stateRefreshTimer !== undefined) {
			clearInterval(this.#stateRefreshTimer);
			this.#stateRefreshTimer = undefined;
		}
	}

	#clearTimers(): void {
		this.#clearStateRefresh();
		if (this.#reconnectTimer !== undefined) {
			clearTimeout(this.#reconnectTimer);
			this.#reconnectTimer = undefined;
		}
	}
}
