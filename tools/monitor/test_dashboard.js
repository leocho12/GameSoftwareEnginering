"use strict";
// DOM behavior checks without launching the game or browser.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

class Element {
    constructor(tag, text = "") { this.tag = tag; this.textContent = text; this.children = []; this.style = {}; this.value = ""; }
    append(...items) { this.children.push(...items); }
    replaceChildren(...items) { this.children = items; }
    setAttribute() {}
    addEventListener() {}
}
const elements = new Map();
const document = {
    getElementById(id) {
        if (!elements.has(id)) elements.set(id, new Element("div"));
        return elements.get(id);
    },
    createElement(tag) { return new Element(tag); },
};
let opened = null;
const context = vm.createContext({
    document, Option: class extends Element {
        constructor(text, value) { super("option", text); this.value = value; }
    },
    window: { location: { href: "http://127.0.0.1:8765/" }, open(...args) { opened = args; return { focus() {} }; } },
    AbortController, Blob, URL, setTimeout() {}, clearTimeout() {},
    fetch() { return new Promise(() => {}); },
});
const html = fs.readFileSync(path.join(__dirname, "index.html"), "utf8");
vm.runInContext(html.split("<script>")[1].split("</script>")[0], context);
function text(element) { return element.textContent + element.children.map(text).join(" "); }
const village = {
    id: "frontier", name: "변경 마을", population: 22, npcs_registered: 22,
    npcs_present: 19, npcs_outside: 1, npcs_remote: 2, npcs_injured: 1,
    atmosphere: { label: "걱정", reason: "부상 인원 발생" },
    tendencies: { rescue_priority: 0.75, command_obedience: 0.4, autonomy: 0.6 },
    rumors: [{ text: "수색 소식", holders: 1, confidence: 0.7, sources: ["벨"] }],
};
const data = {
    game: { world_ready: true, world_clock: "1일 9:00", paused: false },
    counts: { npcs_total: 22, npcs_active: 22, npcs_remote: 2, npcs_incapacitated: 0, mobs_alive: 30, mob_slots: 32, mobs_respawning: 2 },
    villages: [village, { id: "empty", name: "시험용 빈 마을" }],
    npcs: [
        { id: "town:0", name: "상인", role: "상인", state: "업무", action: "거래", hp: 100, max_hp: 100, village_id: "frontier", available: true, position: { x: 1, z: 2 }, memories: 1, beliefs: 0 },
        { id: "world:1001", name: "미라", role: "동료", state: "원정", action: "구조", hp: 75, max_hp: 100, village_id: "frontier", available: false, position: null, memories: 2, beliefs: 1 },
    ],
    transport: { stale: false, age_ms: 100, source: "test.json" },
};
context.render(data);
assert.match(text(elements.get("villages")), /걱정/);
assert.match(text(elements.get("villages")), /75%/);
assert.match(text(elements.get("villages")), /수색 소식/);
assert.match(text(elements.get("villages")), /확신 70%/);
assert.match(text(elements.get("villages")), /미수신/); // Old snapshots remain supported.
assert.equal(elements.get("rows").children.length, 2);
assert.equal(elements.get("bandit-rows").children.length, 0);
data.objective = { title: "3/3 마을 경비에게 복귀 보고" };
data.bandits = [{ id: 0, band_id: 1, hp: 26, max_hp: 40, action_label: "관찰",
    aggression: .2, risk_tolerance: .3, provoked: false, reason: "highest_utility",
    scores: [.35, .6, .3, -1, -1] }];
data.story = { status: "현장 순찰 진행", next_step: "미라에게 F 대화" };
data.economy = { player_scrap: 10, recent_events: [{kind:"robbery",time_s:12,from:1,to:11,amount:3}] };
data.npcs[0].wallet_scrap = 20;
data.npcs[0].potion_stock = 5;
data.npcs[0].medical_supplies = 0;
context.render(data);
assert.match(elements.get("story-status").textContent, /미라에게 F/);
assert.match(elements.get("economy-events").textContent, /약탈/);
assert.match(text(elements.get("rows")), /회복약 5/);
assert.match(elements.get("patrol-objective").textContent, /복귀 보고/);
assert.equal(elements.get("bandit-rows").children.length, 1);
assert.match(text(elements.get("bandit-rows")), /2조/);
assert.match(text(elements.get("bandit-rows")), /행동 점수 우선/);
assert.match(text(elements.get("bandit-rows")), /경계 0.60/);

elements.get("filter").value = "remote";
context.renderRows();
assert.equal(elements.get("rows").children.length, 1);
vm.runInContext('selected = "world:1001"; showDetail();', context);
assert.match(elements.get("detail-json").textContent, /미라/);
elements.get("open-window").onclick();
assert.match(opened[2], /popup/);
assert.equal(opened[0], "http://127.0.0.1:8765/");
data.transport.stale = true;
context.render(data);
assert.match(elements.get("connection").textContent, /수신 중단/);
console.log("Dashboard: village summaries, compatibility, filters, remote detail, popup, stale state passed.");
