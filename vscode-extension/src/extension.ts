import * as vscode from "vscode";

import { type ConnectionState, PipeClient } from "./pipeClient";
import { isPrivacyRule, matchesFile, type PrivacyRule } from "./rules";

const CONFIGURATION_SECTION = "obsPrivacyGuard";

export function activate(context: vscode.ExtensionContext): void {
	// Create the user-visible diagnostics and connection status indicators.
	const log = vscode.window.createOutputChannel("OBS Privacy Guard", { log: true });
	const statusBar = vscode.window.createStatusBarItem(
		vscode.StatusBarAlignment.Left,
		100,
	);
	statusBar.name = "OBS Privacy Guard";
	statusBar.show();

	let connectionState: ConnectionState = "disconnected";
	let sensitive = true;

	// Keep the status bar aligned with both the file scan and pipe connection state.
	const renderStatus = (): void => {
		if (sensitive) {
			statusBar.text = "$(shield) OBS: hidden";
			statusBar.tooltip = "A visible file matches an OBS Privacy Guard rule.";
			statusBar.backgroundColor = new vscode.ThemeColor(
				"statusBarItem.warningBackground",
			);
			return;
		}

		statusBar.backgroundColor = undefined;
		if (connectionState === "connected") {
			statusBar.text = "$(shield) OBS: safe";
			statusBar.tooltip = "OBS Privacy Guard is connected and no visible file matches.";
		} else {
			statusBar.text = "$(debug-disconnect) OBS: disconnected";
			statusBar.tooltip = "OBS Privacy Guard is disconnected; OBS should fail closed.";
		}
	};

	// Maintain the local named-pipe connection to the OBS plugin.
	const client = new PipeClient(log, (state) => {
		connectionState = state;
		renderStatus();
	});

	// Re-evaluate every editor currently visible in any VS Code editor group.
	const evaluateVisibleEditors = (): void => {
		const configuration = vscode.workspace.getConfiguration(
			CONFIGURATION_SECTION,
		);
		const enabled = configuration.get<boolean>("enabled", true);
		const configuredRules = configuration.get<unknown[]>("rules", []);
		const rules: readonly PrivacyRule[] = configuredRules.filter(isPrivacyRule);

		if (configuredRules.length !== rules.length) {
			log.warn("One or more privacy rules are invalid and were ignored.");
		}

		// Reporting sensitive while locally disabled ensures VS Code cannot disarm
		// the OBS-side safety layer. OBS must be disabled explicitly in OBS.
		sensitive = !enabled || vscode.window.visibleTextEditors.some((editor) => matchesFile(editor.document.uri.path, rules));

		client.publish(sensitive ? "sensitive" : "safe");
		renderStatus();
	};

	// Re-scan when visible files or Privacy Guard configuration changes.
	context.subscriptions.push(
		log,
		statusBar,
		vscode.window.onDidChangeVisibleTextEditors(evaluateVisibleEditors),
		vscode.workspace.onDidChangeConfiguration((event) => {
			if (event.affectsConfiguration(CONFIGURATION_SECTION)) {
				evaluateVisibleEditors();
			}
		}),
		{ dispose: () => client.dispose() },
	);

	// Publish a fail-closed initial state before opening the transport.
	evaluateVisibleEditors();
	client.connect();
}
