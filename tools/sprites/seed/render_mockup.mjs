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
const LYING = {
  couch: { by: 43, top: P.tee, topS: P.teeS, shorts: P.shorts, shortsS: P.shortsS, hold: 'manga' },
  beach: { by: 41, top: P.S, topS: P.skinS, shorts: P.swim, shortsS: P.swimS, hold: 'colada', topless: true },
};
const LYING_HEAD = (ly) => [-11, ly.by - 35];

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
      const ly = LYING[req.pose], [hx, hy] = LYING_HEAD(ly);
      M.drawLying(L, ly, 'body');
      M.drawTomo(L, { ox: hx, oy: hy, face: req.face, item: req.item, noBody: true });
      if (!req.item) M.drawLying(L, ly, 'hold');
      L.outline(P.OL);
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
