// SPDX-License-Identifier: GPL-3.0-only
// EDDA OS: node web/test_onefile.mjs OUT.html (web/make_site.py --onefile's page; tests/run_tests.sh makes one from the package)
// The one-file installer (web/make_site.py --onefile) run as a browser would: its own script, a DOM stand-in, Web MIDI
// from a simulated FM-1 on stock firmware (FM-1_015) that switches to Felucca's loader (ota-FM-1_900) and comes back
// as the installed package's identity. Checks: the embedded package is read (no fetch), Install writes the image the
// device asks for byte for byte, the page ends on "Done". 2.0, a page with a sound pack in it (make_site.py --onefile
// .. PACK_DIR): its second button refuses a device that does not run this package yet, and after the install loads
// the pack over the editor's sample commands (the fake device keeps what it is sent as the firmware would: BEGIN,
// the writes in order from 512, END with a header whose CRC matches) into slots that end up the pack's, byte for byte.
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
  ed(cmd, a) {                                       /* the editor protocol's sample commands (firmware/src/editor.c) */
    const reply = (...b) => this.tx([0xF0, 0x7D, 0x46, 0x4C, cmd, ...b, 0xF7]);
    const size = (k) => (k === 3 ? 73728 : 81920), slot = a[0];
    this.slots ||= [null, null, null, null]; this.hdrs ||= [null, null, null, null]; this.edErr ||= 0;
    if (!this.identity.startsWith("FM-1_9") || slot > 3) { this.edErr++; return; }
    if (cmd === 11) { this.slots[slot] = { data: new Uint8Array(size(slot)).fill(0xFF), next: 512 }; this.hdrs[slot] = null; reply(slot, 0); }
    else if (cmd === 12) {
      const off = a[1] | a[2] << 7 | a[3] << 14, s = this.slots[slot];
      let chunk = null;
      try { chunk = unpackBk(a.slice(4)); } catch (e) { chunk = null; }
      let rc = 0;
      if (!s || this.hdrs[slot]) rc = 4;
      else if (!chunk || !chunk.length || chunk.length > 256 || off < 512 || (off & 255) || off !== s.next || off + chunk.length > size(slot)) rc = 1;
      else { s.data.set(chunk, off); s.next = off + 256; }
      if (rc) this.edErr++;
      reply(slot, off & 127, off >>> 7 & 127, off >>> 14 & 127, rc);
    } else if (cmd === 13) {
      let h = null, rc = 0;
      try { h = unpackBk(a.slice(1)); } catch (e) { h = null; }
      const s = this.slots[slot];
      if (!h || h.length !== 480 || !s) rc = 1;
      else {
        const v = new DataView(h.buffer), len = v.getUint32(16, true);
        if (v.getUint32(0, true) !== 0x504d5346 || v.getUint16(4, true) !== 1 || !h[6] || h[6] > 16 || len > size(slot) - 512) rc = 2;
        else if (crc32(s.data.subarray(512, 512 + len)) !== v.getUint32(20, true)) rc = 3;
        else { s.data.set(h, 0); this.hdrs[slot] = h; }
      }
      if (rc) this.edErr++;
      reply(slot, rc);
    } else this.edErr++;
  }
  rx(d) {
    if (d.length >= 6 && d[0] === 0xF0 && d[1] === 0x7D && d[2] === 0x46 && d[3] === 0x4C && d.at(-1) === 0xF7) {
      this.ed(d[4], d.slice(5, -1));
    } else if (eq(d, HS)) {
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

function crc32(bytes) { let c = 0xffffffff; for (const b of bytes) { c ^= b; for (let k = 0; k < 8; k++) c = c >>> 1 ^ (c & 1 ? 0xedb88320 : 0); } return (~c) >>> 0; }
function unpackBk(a) {                                /* groups of a mask byte and up to 7 bytes (editor.c ed_unpack7) */
  const out = []; let i = 0;
  while (i < a.length) {
    const m = a[i++], k = Math.min(7, a.length - i);
    if (!k || m >>> k) throw new Error("pack7");
    for (let j = 0; j < k; j++) out.push(a[i++] | (m >>> j & 1) << 7);
  }
  return Uint8Array.from(out);
}
const packSlots = meta.pack ? meta.pack.slots.map((b) => (b ? Uint8Array.from(atob(b), (c) => c.charCodeAt(0)) : null)) : null;

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
let packOk = true;
if (packSlots) {                                      /* the sounds before the install: refused, nothing sent */
  const shown = els.pack.hidden === false && !els["pack-go"].disabled && els["pack-go"].textContent.includes(meta.pack.song);
  els["pack-go"].click();
  const refused = await wait(() => /^Install from this page first/.test(els.status.textContent), 5000);
  await wait(() => !els.go.disabled, 5000);
  console.log(`sound pack ${meta.pack.name} ("${meta.pack.song}"): button shown ${shown}; before the install: "${els.status.textContent}", sample commands the device got ${fm.slots ? "some" : "none"}`);
  packOk = shown && refused && !fm.slots;
}
const before = els.status.textContent;
els.go.click();
const done = await wait(() => els.status.textContent === "Done" || (els.status.textContent !== before && /Error|not|Stopped|reports/.test(els.status.textContent)), 120000);
console.log(`after Install: status "${els.status.textContent}", bar ${els.bar.value}, device ${fm.identity}, reads ${fm.served}, bytes checked ${fm.bytes}, mismatches ${fm.bad}`);
if (packSlots && els.status.textContent === "Done") {  /* .. and after it: the slots are the pack's */
  await wait(() => !els["pack-go"].disabled, 5000);
  els["pack-go"].click();
  const loaded = await wait(() => /^Sounds loaded/.test(els.status.textContent) || /Error|not found|first/.test(els.status.textContent), 300000);
  let same = 0, bytes = 0;
  packSlots.forEach((b, k) => {
    const s = fm.slots && fm.slots[k];
    if (!b) { same += !s; return; }
    const ok = s && fm.hdrs[k] && b.subarray(0, 480).every((x, i) => x === s.data[i]) && b.subarray(512).every((x, i) => x === s.data[512 + i]);
    same += ok ? 1 : 0; bytes += b.length;
  });
  console.log(`after the sounds: status "${els.status.textContent}", bar ${els.bar.value}, slots the pack's ${same} of 4 (${bytes} B), device errors ${fm.edErr || 0}`);
  packOk = packOk && loaded && /^Sounds loaded/.test(els.status.textContent) && same === 4 && !(fm.edErr || 0) && els.bar.value === 100;
}
const ok = ready && fetched === 0 && (packSlots ? packOk : els.status.textContent === "Done") && fm.identity === meta.product && fm.bad === 0 && els.bar.value === 100;
console.log(ok ? "one-file installer: ok" : "one-file installer: FAIL");
process.exit(ok ? 0 : 1);
