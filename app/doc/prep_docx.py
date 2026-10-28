#!/usr/bin/env python3
"""Turn doc/out/essay.json (+ fig_*.svg) into doc/out/docx_src.json and PNG figures for build_docx.js.

The essay is written once (doc/essay.py); build.py renders it into the app and writes essay.json.
This step only flattens the inline HTML (<b>, <code>) into runs and rasterises the same SVGs.
"""
import json, re, html, os, cairosvg
AB = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = f'{AB}/doc/out'
E = json.load(open(f'{OUT}/essay.json'))

def runs(h):
    out, bold, code = [], False, False
    for tok in re.split(r'(</?b>|</?code>)', h):
        if tok == '<b>': bold = True
        elif tok == '</b>': bold = False
        elif tok == '<code>': code = True
        elif tok == '</code>': code = False
        elif tok:
            assert '<' not in tok, tok
            out.append(dict(t=html.unescape(tok), b=bold, c=code))
    return out

blocks = []
for b in E['blocks']:
    k = b['kind']
    if k == 'h2': blocks.append(dict(k='h2', t=b['text']))
    elif k == 'p': blocks.append(dict(k='p', r=runs(b['html'])))
    elif k in ('ul', 'ol'): blocks.append(dict(k=k, items=[runs(i) for i in b['items']]))
    elif k == 'code': blocks.append(dict(k='code', lines=b['text'].split('\n')))
    elif k == 'table': blocks.append(dict(k='table', head=b['head'], rows=[[html.unescape(re.sub('<[^>]+>', '', c)) for c in r] for r in b['rows']]))
    elif k == 'fig':
        svg = open(f'{OUT}/fig_{b["key"]}.svg').read()
        m = re.search(r'viewBox="0 0 ([\d.]+) ([\d.]+)"', svg)
        w, h = float(m.group(1)), float(m.group(2))
        png = f'{OUT}/fig_{b["key"]}.png'
        cairosvg.svg2png(bytestring=svg.encode(), write_to=png, output_width=int(w * 2.5), output_height=int(h * 2.5))
        blocks.append(dict(k='fig', png=png, w=w, h=h, cap=b['caption']))
    else:
        raise SystemExit('unknown block ' + k)
json.dump(dict(title=E['title'], subtitle=E['subtitle'], blocks=blocks), open(f'{OUT}/docx_src.json', 'w'), indent=1)
print(len(blocks), 'blocks;', sum(1 for b in blocks if b['k'] == 'fig'), 'figures')
