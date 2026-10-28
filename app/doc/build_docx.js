// Build swmm5-cplus-doc.docx from doc/out/docx_src.json (same essay as the app's Doc tab).
const fs = require('fs');
const path = require('path');
const {
  Document, Packer, Paragraph, TextRun, HeadingLevel, ImageRun, Table, TableRow, TableCell,
  WidthType, ShadingType, BorderStyle, AlignmentType, LevelFormat, Footer, PageNumber,
} = require('docx');

const OUT = path.join(__dirname, 'out');
const src = JSON.parse(fs.readFileSync(path.join(OUT, 'docx_src.json'), 'utf8'));
const dest = process.argv[2] || path.join(OUT, 'swmm5-cplus-doc.docx');

const BODY = 'Georgia', HEAD = 'Arial', MONO = 'Consolas';
const INK = '1B2733', DIM = '5A6878', ACCENT = '1F5FAD';
const CONTENT_W = 9360; // 12240 - 2 * 1440

const run = (r, extra = {}) => new TextRun({
  text: r.t, bold: r.b || extra.bold, font: r.c ? MONO : (extra.font || BODY),
  size: r.c ? 19 : (extra.size || 22), color: extra.color || INK,
});

const children = [];
children.push(new Paragraph({ spacing: { after: 120 }, children: [new TextRun({ text: src.title, font: HEAD, size: 44, bold: true, color: INK })] }));
children.push(new Paragraph({ spacing: { after: 120 }, children: [new TextRun({ text: src.subtitle, font: HEAD, size: 26, color: DIM })] }));
children.push(new Paragraph({
  spacing: { after: 360 },
  border: { bottom: { style: BorderStyle.SINGLE, size: 6, color: 'C9D3DE', space: 8 } },
  children: [new TextRun({ text: 'The essay from the Doc tab of the swmm5-cplus app, October 2026', font: HEAD, size: 18, color: DIM })],
}));

let listN = 0;
const numberingConfig = [{
  reference: 'bullets',
  levels: [{ level: 0, format: LevelFormat.BULLET, text: '•', alignment: AlignmentType.LEFT,
    style: { paragraph: { indent: { left: 540, hanging: 270 } } } }],
}];

for (const b of src.blocks) {
  if (b.k === 'h2') {
    children.push(new Paragraph({ heading: HeadingLevel.HEADING_1, children: [new TextRun({ text: b.t })] }));
  } else if (b.k === 'p') {
    const nxt = src.blocks[src.blocks.indexOf(b) + 1];
    children.push(new Paragraph({ spacing: { after: 160, line: 300 }, keepNext: !!(nxt && nxt.k === 'code'), children: b.r.map(r => run(r)) }));
  } else if (b.k === 'ul' || b.k === 'ol') {
    let ref = 'bullets';
    if (b.k === 'ol') {
      ref = 'steps' + (++listN); // each numbered list gets its own reference so it restarts at 1
      numberingConfig.push({ reference: ref, levels: [{ level: 0, format: LevelFormat.DECIMAL, text: '%1.',
        alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 540, hanging: 360 } } } }] });
    }
    for (const it of b.items) {
      children.push(new Paragraph({ numbering: { reference: ref, level: 0 }, spacing: { after: 100, line: 288 }, children: it.map(r => run(r)) }));
    }
  } else if (b.k === 'code') {
    b.lines.forEach((ln, i) => children.push(new Paragraph({
      shading: { type: ShadingType.CLEAR, color: 'auto', fill: 'F1F4F8' },
      spacing: { before: i === 0 ? 60 : 0, after: i === b.lines.length - 1 ? 200 : 0, line: 240 },
      indent: { left: 180, right: 180 }, keepLines: true, keepNext: i < b.lines.length - 1,
      children: [new TextRun({ text: ln.length ? ln : ' ', font: MONO, size: 17, color: INK })],
    })));
  } else if (b.k === 'table') {
    const widths = [1300, 4460, 1700, 1900];
    const cell = (t, i, head) => new TableCell({
      width: { size: widths[i], type: WidthType.DXA },
      shading: head ? { type: ShadingType.CLEAR, color: 'auto', fill: 'E4EAF2' } : undefined,
      margins: { top: 60, bottom: 60, left: 100, right: 100 },
      children: [new Paragraph({ children: [new TextRun({ text: t, font: HEAD, size: 18, bold: head, color: INK })] })],
    });
    children.push(new Table({
      width: { size: CONTENT_W, type: WidthType.DXA }, columnWidths: widths,
      rows: [new TableRow({ tableHeader: true, children: b.head.map((t, i) => cell(t, i, true)) }),
        ...b.rows.map(r => new TableRow({ children: r.map((t, i) => cell(t, i, false)) }))],
    }));
    children.push(new Paragraph({ spacing: { after: 160 }, children: [] }));
  } else if (b.k === 'fig') {
    const wpx = 624, hpx = Math.round(wpx * b.h / b.w);
    children.push(new Paragraph({ alignment: AlignmentType.CENTER, spacing: { before: 120, after: 80 }, keepNext: true,
      children: [new ImageRun({ type: 'png', data: fs.readFileSync(b.png), transformation: { width: wpx, height: hpx },
        altText: { title: b.cap.split('.')[0], description: b.cap, name: path.basename(b.png) } })] }));
    children.push(new Paragraph({ spacing: { after: 240, line: 264 },
      children: [new TextRun({ text: b.cap, font: HEAD, size: 18, italics: true, color: DIM })] }));
  }
}

const doc = new Document({
  creator: 'swmm5-cplus build (see AI use)',
  title: src.title,
  description: src.subtitle,
  styles: {
    default: { document: { run: { font: BODY, size: 22, color: INK } } },
    paragraphStyles: [
      { id: 'Heading1', name: 'Heading 1', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { font: HEAD, size: 30, bold: true, color: ACCENT },
        paragraph: { spacing: { before: 360, after: 140 }, keepNext: true, outlineLevel: 0 } },
    ],
  },
  numbering: { config: numberingConfig },
  sections: [{
    properties: { page: { size: { width: 12240, height: 15840 }, margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 } } },
    footers: { default: new Footer({ children: [new Paragraph({ alignment: AlignmentType.CENTER,
      children: [new TextRun({ text: 'Same Numbers, Different Bones · ', font: HEAD, size: 16, color: DIM }),
        new TextRun({ children: [PageNumber.CURRENT], font: HEAD, size: 16, color: DIM })] })] }) },
    children,
  }],
});

Packer.toBuffer(doc).then(buf => { fs.writeFileSync(dest, buf); console.log('wrote', dest, buf.length, 'bytes'); });
