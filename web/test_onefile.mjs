// SPDX-License-Identifier: GPL-3.0-only
// EDDA OS: node web/test_onefile.mjs OUT.html (web/make_site.py --onefile's page; tests/run_tests.sh makes one from the package)
// The one-file installer (web/make_site.py --onefile) run as a browser would: its own script, a DOM stand-in, Web MIDI
// from a simulated FM-1 on stock firmware (FM-1_015) that switches to Felucca's loader (ota-FM-1_900) and comes back
// as the installed package's identity. Checks: the embedded package is read (no fetch), Install writes the image the
// device asks for byte for byte, the page ends on "Done".
import { readFileSync } from "node:fs";
import vm from "node:vm";
const html = readFileSync(process.argv[2], "utf8");
const script = html.match(/<script type="module">([\s\S]*?)<\/script>/)[1];
const meta = JSON.parse(script.match(/const meta = (\{.*?\});/s)[1]);
const pkg = Uint8Array.from(atob(meta.b64), (c) => c.charCodeAt(0));
const BLOCKS = 20, BLK = 0x30, KEEP = 0x2F;
const logical = new Uint8Array(pkg.length - BLOCKS);
for (let i = 0; i < BLOCKS; i++) logical.set(pkg.subarray(i * BLK, i * BLK + KEEP), i * KEEP);
logical.set(pkg.subarray(BLOCKS * BLK), BLOCKS * KEEP);

// ---- the fake FM-1 (as web/test_web.mjs FakeFM1), its final identity the package's
const eq = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
let pack7, unpack7;                                   /* (the page's own, once its script has run) */
const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7], UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
class FakeFM1 {
  constructor(image, finalId) { this.image = image; this.finalId = finalId; this.served = 0; this.bad = 0; this.bytes = 0; this.access = { inputs: new Map(), outputs: new Map() }; this.boot("FM-1_015", "FM-1"); }
  boot(identity, name) {
    this.identity = identity; this.waiting = null; this.queue = [];
    for (const m of [this.access.inputs, this.access.outputs]) { for (const p of m.values()) p.state = "disconnected"; m.clear(); }
    const id = Math.random().toString(36).slice(2);
    this.input = { id: "i" + id, name, state: "connected", onmidimessage: null, open: async () => {}, close: async () => {} };
    this.output = { id: "o" + id, name, state: "connected", open: async () => {}, close: async () => {}, send: (d) => {
      if (this.output.state !== "connected") throw new Error("InvalidStateError");
      setTimeout(() => this.rx(Array.from(d)), 1); } };
    this.access.inputs.set(this.input.id, this.input); this.access.outputs.set(this.output.id, this.output);
  }
  tx(bytes) { const i = this.input; setTimeout(() => { if (i.state === "connected" && i.onmidimessage) i.onmidimessage({ data: Uint8Array.from(bytes) }); }, 1); }
  rx(d) {
    if (eq(d, HS)) {
      const t = [...new TextEncoder().encode(this.identity)];
      this.tx([0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
    } else if (eq(d, UPGRADE)) {
      this.queue = this.identity.startsWith("ota-")
        ? [...Array.from({ length: 24 }, (_, k) => [k * 512, 512]), [0xF0000000, 8]]
        : [[0, 64], [0x40, 160], [0x1000, 512], [0xE0000000, 8]];
      this.next();
    } else if (this.waiting) {
      const u = unpack7(d.slice(1, -1)), [addr, len] = this.waiting;
      const got = u.slice(14, 14 + (addr >= 0xE0000000 ? 8 : len));
      const want = addr >= 0xE0000000 ? [...new TextEncoder().encode("success"), 0] : Array.from(this.image.subarray(addr, addr + len));
      if (!eq(got, want)) this.bad++;
      this.bytes += addr >= 0xE0000000 ? 0 : len;
      this.waiting = null; this.served++;
      if (addr === 0xE0000000) setTimeout(() => this.boot("ota-FM-1_900", "Felucca Update"), 300);
      else if (addr === 0xF0000000) setTimeout(() => this.boot(this.finalId, "Felucca"), 300);   // (usb.c STR2: the app is "Felucca")
      else this.next();
    }
  }
  next() {
    const r = this.queue.shift(); if (!r) return; this.waiting = r; const [addr, len] = r;
    const u = [0, 0x59, 0x30, 0, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, len >> 8, 0];
    let s = 0; for (let i = 6; i < 14; i++) s += u[i]; u.push(~s & 0xFF);
    this.tx([0xF0, ...pack7(u), 0xF7]);
  }
}

// ---- a DOM stand-in: the page's elements by id and by data-t, from its own markup
const els = {};
const mk = (id, tag = "x", dt) => (els[id] = { id, tag, textContent: "", disabled: false, value: 0, checked: false, files: [], dataset: dt ? { t: dt } : {}, h: {},
  addEventListener(t, f) { (this.h[t] ||= []).push(f); }, click() { (this.h.click || []).forEach((f) => f()); } });
for (const m of html.matchAll(/<(\w+)[^>]*\bid="([^"]+)"[^>]*>/g)) mk(m[2], m[1]);
const dataT = [...html.matchAll(/<(\w+)[^>]*\bdata-t="([^"]+)"[^>]*>/g)].map((m, i) => {
  const idm = m[0].match(/\bid="([^"]+)"/); const e = idm ? els[idm[1]] : mk("_dt" + i, m[1]); e.dataset.t = m[2]; return e; });
const h1 = mk("_h1", "h1");
const fm = new FakeFM1(logical, meta.product);
let fetched = 0;
const ctx = {
  console, setTimeout, clearTimeout, setInterval, clearInterval, TextEncoder, TextDecoder, Uint8Array, Promise, atob, URL, Blob,
  fetch: async () => { fetched++; throw new Error("fetch called"); },
  navigator: { language: "en", requestMIDIAccess: async (o) => { if (!o || !o.sysex) throw new Error("sysex"); return fm.access; } },
  window: { addEventListener() {} }, confirm: () => true,
  document: { title: "", documentElement: { lang: "" }, getElementById: (id) => els[id] || null, querySelectorAll: () => dataT,
    querySelector: (q) => (q === "h1" ? h1 : null), createElement: () => mk("_a"), body: { append() {} } },
};
ctx.globalThis = ctx;
vm.createContext(ctx);
vm.runInContext(script, ctx, { filename: "installer" });
pack7 = (u) => Array.from(vm.runInContext("pack7", ctx)(Uint8Array.from(u)));
unpack7 = (p) => Array.from(vm.runInContext("unpack7", ctx)(Uint8Array.from(p)));
const wait = async (f, ms) => { const t0 = Date.now(); while (!f()) { if (Date.now() - t0 > ms) return false; await new Promise((r) => setTimeout(r, 20)); } return true; };
const ready = await wait(() => !els.go.disabled, 5000);
console.log(`loaded: ${ready}, title "${ctx.document.title}", h1 "${h1.textContent}", status "${els.status.textContent}", fetch calls ${fetched}`);
els.go.click();
const done = await wait(() => els.status.textContent === "Done" || /Error|not|Stopped|reports/.test(els.status.textContent), 120000);
console.log(`after Install: status "${els.status.textContent}", bar ${els.bar.value}, device ${fm.identity}, reads ${fm.served}, bytes checked ${fm.bytes}, mismatches ${fm.bad}`);
const ok = ready && fetched === 0 && els.status.textContent === "Done" && fm.identity === meta.product && fm.bad === 0 && els.bar.value === 100;
console.log(ok ? "one-file installer: ok" : "one-file installer: FAIL");
process.exit(ok ? 0 : 1);
