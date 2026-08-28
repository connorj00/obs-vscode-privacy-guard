import { win32 } from "node:path";

// Rule names match the values exposed by the VS Code configuration schema.
const RULE_TYPES = [
	"extension",
	"startsWith",
	"endsWith",
	"includes",
] as const;

export type RuleType = (typeof RULE_TYPES)[number];

export interface PrivacyRule {
	readonly id: string;
	readonly type: RuleType;
	readonly value: string;
	readonly caseSensitive?: boolean;
}

// Reject malformed user settings before they reach the filename matcher.
export function isPrivacyRule(value: unknown): value is PrivacyRule {
	if (typeof value !== "object" || value === null) {
		return false;
	}

	const candidate = value as Record<string, unknown>;
	return (
		typeof candidate.id === "string" && candidate.id.trim().length > 0 &&
		typeof candidate.value === "string" && candidate.value.length > 0 &&
		typeof candidate.type === "string" && RULE_TYPES.includes(candidate.type as RuleType) &&
		(candidate.caseSensitive === undefined || typeof candidate.caseSensitive === "boolean"));
}

export function matchesFile(
	filePath: string,
	rules: readonly PrivacyRule[],
): boolean {
	// Rules intentionally inspect only the basename, never file contents or paths.
	const fileName = win32.basename(filePath);
	return rules.some((rule) => matchesRule(fileName, rule));
}

function matchesRule(fileName: string, rule: PrivacyRule): boolean {
	// Normalize both values once so each rule type has identical case semantics.
	const actual = rule.caseSensitive ? fileName : fileName.toLowerCase();
	const expected = rule.caseSensitive ? rule.value : rule.value.toLowerCase();

	switch (rule.type) {
		case "extension": {
			const normalized = expected.startsWith(".") ? expected : `.${expected}`;
			return actual === normalized || actual.endsWith(normalized);
		}
		case "startsWith":
			return actual.startsWith(expected);
		case "endsWith":
			return actual.endsWith(expected);
		case "includes":
			return actual.includes(expected);
	}
}
