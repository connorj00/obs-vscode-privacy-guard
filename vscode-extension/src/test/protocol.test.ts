import assert from "node:assert/strict";
import { describe, it } from "node:test";

import {
	goodbyeFrame,
	helloFrame,
	stateFrame,
} from "../protocol";

describe("protocol frames", () => {
	it("serializes the versioned handshake and state without file data", () => {
		assert.equal(helloFrame("client-id"), "PG/1 HELLO client-id\n");
		assert.equal(stateFrame(1, "safe"), "PG/1 STATE 1 SAFE\n");
		assert.equal(stateFrame(2, "sensitive"), "PG/1 STATE 2 SENSITIVE\n");
	});

	it("serializes the shutdown frame", () => {
		assert.equal(goodbyeFrame(), "PG/1 GOODBYE\n");
	});
});
