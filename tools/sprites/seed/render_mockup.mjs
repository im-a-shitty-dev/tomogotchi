// Render named layers from the character mockup (design/tomo-moods.html).
//
// Helper for seed.py: it pulls the drawing code out of the mockup, runs it
// without a browser, and prints { name: [52*64 colours, "#rrggbb" or null] }
// as JSON.
//
//   node render_mockup.mjs path/to/tomo-moods.html requests.json
//
// requests.json is a list of { name, kind, ... } written by seed.py.
import { readFileSync } from 'node:fs';

const [html, reqPath] = process.argv.slice(2);
const src = readFileSync(html, 'utf8');
const start = src.indexOf('const W = 52');
const end = src.indexOf('const cv = document');
const blanket = src.match(/function drawBlanket\(L\) \{[\s\S]*?\n\}/);
if (start < 0 || end < 0 || !blanket) throw new Error('mockup layout changed; update render_mockup.mjs');

const code = `let t = 0, scene = 'bath';
${src.slice(start, end)}
${blanket[0]}
return {
  W, H, P, Layer, drawItem, drawTomo, drawScene, drawTub, drawBlanket, drawLying,
  set: (tt, sc) => { t = tt; scene = sc; },
};`;
const M = new Function(code)();
const P = M.P;

// Lying poses, as set up in the mockup's frame(). Keep in sync with it.
// ox shifts the whole pose sideways (the mockup's drawLying has none built in).
const LYING = {
  couch: { by: 42, ox: -1, top: P.tee, topS: P.teeS, shorts: P.shorts, shortsS: P.shortsS, hold: 'manga' },
  beach: { by: 38, top: P.S, topS: P.skinS, shorts: P.swim, shortsS: P.swimS, hold: 'colada', topless: true },
};
const LYING_HEAD = (ly) => [-11 + (ly.ox || 0), ly.by - 35];

function render(req) {
  const L = new M.Layer();
  M.set(req.t || 0, req.scene || 'bath');
  switch (req.kind) {
    case 'scene': {
      // Leave out things that move freely (bats); the firmware draws those.
      const px = L.px.bind(L), skip = new Set(req.skip || []);
      L.px = (x, y, c) => { if (!skip.has(c)) px(x, y, c); };
      M.drawScene(L, req.mood, req.t || 0);
      break;
    }
    case 'tomo': M.drawTomo(L, req.opts); L.outline(P.OL); break;
    case 'lying': {
      const ly = LYING[req.pose], [hx, hy] = LYING_HEAD(ly), ox = ly.ox || 0;
      M.drawLying(L, ly, 'body');
      M.drawTomo(L, { ox: hx - ox, oy: hy, face: req.face, item: req.item, noBody: true });
      if (!req.item) M.drawLying(L, ly, 'hold');
      L.outline(P.OL);
      if (ox) {  // slide everything sideways
        const a = L.a.slice();
        L.a.fill(null);
        for (let i = 0; i < a.length; i++) if (a[i]) L.px(i % M.W + ox, Math.floor(i / M.W), a[i]);
      }
      break;
    }
    // Free-moving bits the mockup draws inline in frame(). Copied from there;
    // keep in sync. Each is drawn on an empty layer, without outline.
    case 'steam': {
      const t = req.t;
      for (const [x, ph] of [[10, 0], [19, 7], [34, 3]]) { const s = (t + ph * 3) % 20; if (s < 13) L.px(x + (s % 6 < 3 ? 0 : 1), 36 - s, '#4f7f87'); }
      break;
    }
    case 'zzz': {
      const t = req.t;
      for (let i = 0; i < 3; i++) {
        const s = (t + i * 10) % 30; if (s > 22) continue;
        const zx = 37 + i * 3 + Math.floor(s / 8), zy = 16 - Math.floor(s / 2), c = s > 16 ? '#5a5f80' : '#b9c2e8';
        L.rect(zx, zy, 4, 1, c); L.px(zx + 2, zy + 1, c); L.px(zx + 1, zy + 2, c); L.rect(zx, zy + 3, 4, 1, c);
      }
      break;
    }
    case 'bat': {
      // One bat crossing the sky (the mockup's first bat, without its bob).
      const t = req.t, bx = (t % 66) - 7, by = 7;
      const wy = by + (t % 4 < 2 ? -1 : 1), c = '#05030a';
      L.px(bx, by, c); L.px(bx, by + 1, c);
      [-2, -1, 1, 2].forEach(dx => L.px(bx + dx, wy, c));
      break;
    }
    case 'splash': {
      // Drops flicking out of the tub when he's shaken.
      for (const [x, y] of [[2, 37], [3, 35], [49, 36], [48, 34], [10, 35], [41, 34]]) L.px(x, y - (req.t % 2), '#9fd8ff');
      break;
    }
    case 'item': M.drawItem(L, req.item, 0, 0); L.outline(P.OL); break;
    case 'tub': M.drawTub(L, 0, false, false); L.outline(P.OL); break;
    case 'blanket': M.drawBlanket(L); L.outline(P.OL); break;
    default: throw new Error(`unknown kind ${req.kind}`);
  }
  return L.a;
}

const out = { _lying_head: Object.fromEntries(Object.entries(LYING).map(([k, ly]) => [k, LYING_HEAD(ly)])) };
for (const req of JSON.parse(readFileSync(reqPath, 'utf8'))) out[req.name] = render(req);
process.stdout.write(JSON.stringify(out));
