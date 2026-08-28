import assert from "node:assert/strict";
import { describe, it } from "node:test";

import { isPrivacyRule, matchesFile, type PrivacyRule } from "../rules";

describe("matchesFile", () => {
	const rules: readonly PrivacyRule[] = [
		{ id: "env", type: "extension", value: ".env" },
		{ id: "server-prefix", type: "startsWith", value: "sv_" },
		{ id: "server-suffix", type: "endsWith", value: "_sv.lua" },
		{ id: "config", type: "includes", value: "config" },
	];

	it("matches dotfiles and conventional extensions", () => {
		assert.equal(matchesFile("C:/project/.env", rules), true);
		assert.equal(matchesFile("C:\\project\\example.env", rules), true);
	});

	it("matches the basename without leaking or matching parent directories", () => {
		assert.equal(matchesFile("C:/config/src/client.lua", rules), false);
		assert.equal(matchesFile("C:/project/src/config.private.lua", rules), true);
	});

	it("supports prefix and suffix rules", () => {
		assert.equal(matchesFile("C:/project/sv_accounts.lua", rules), true);
		assert.equal(matchesFile("C:/project/accounts_sv.lua", rules), true);
	});

	it("is case-insensitive unless requested otherwise", () => {
		assert.equal(matchesFile("C:/project/CONFIG.lua", rules), true);
		assert.equal(
			matchesFile("C:/project/CONFIG.lua",
				[
					{
						id: "case-sensitive",
						type: "includes",
						value: "config",
						caseSensitive: true,
					},
				]),
			false,
		);
	});
});

describe("isPrivacyRule", () => {
	it("rejects empty and unknown rules", () => {
		assert.equal(isPrivacyRule({ id: "", type: "includes", value: "x" }), false);
		assert.equal(isPrivacyRule({ id: "x", type: "regex", value: ".*" }), false);
		assert.equal(isPrivacyRule({ id: "x", type: "includes", value: "" }), false);
	});
});
