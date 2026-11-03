"""Inspect native PDF wire/text intersections on every schematic page."""
from pathlib import Path
import json
import hashlib
import pymupdf as fitz
ROOT=Path(__file__).resolve().parents[1]
doc=fitz.open(ROOT/'docs/CoralC5-Motor_Schematic.pdf')
findings=[];text_overlaps=[];duplicate_search_spans=0
for index,page in enumerate(doc):
    wires=[]
    for path in page.get_drawings():
        color=path.get('color')
        if not color or any(abs(a-b)>.01 for a,b in zip(color,(0,.588235,0))):continue
        for item in path['items']:
            if item[0]=='l':wires.append((item[1],item[2]))
    all_spans=[];seen=set()
    for block in page.get_text('dict')['blocks']:
        for line in block.get('lines',[]):
            for span in line['spans']:
                text=span['text'].strip()
                if not text:continue
                key=(text,tuple(round(v,2) for v in span['bbox']))
                if key in seen:duplicate_search_spans+=1;continue
                seen.add(key);all_spans.append(span)
                r=fitz.Rect(span['bbox']);r.x0+=.6;r.x1-=.6;r.y0+=.6;r.y1-=.6
                if r.is_empty:continue
                for a,b in wires:
                    horizontal=abs(a.y-b.y)<.05
                    vertical=abs(a.x-b.x)<.05
                    hit=(horizontal and r.y0<a.y<r.y1 and max(min(a.x,b.x),r.x0)<min(max(a.x,b.x),r.x1)) or (vertical and r.x0<a.x<r.x1 and max(min(a.y,b.y),r.y0)<min(max(a.y,b.y),r.y1))
                    if hit:
                        findings.append({'page':index+1,'text':text,'text_mm':[round(v*25.4/72,2) for v in span['bbox']],'wire_mm':[[round(a.x*25.4/72,2),round(a.y*25.4/72,2)],[round(b.x*25.4/72,2),round(b.y*25.4/72,2)]]});break
    for si,a in enumerate(all_spans):
        ra=fitz.Rect(a['bbox'])
        for b in all_spans[si+1:]:
            rb=fitz.Rect(b['bbox']);intersection=ra & rb
            if not intersection.is_empty and intersection.width>.6 and intersection.height>.6 and intersection.get_area()>.28*min(ra.get_area(),rb.get_area()):
                text_overlaps.append({'page':index+1,'first':a['text'],'second':b['text']})
    out=ROOT/'docs/previews';out.mkdir(exist_ok=True)
    page.get_pixmap(matrix=fitz.Matrix(1.7,1.7)).save(str(out/f'page-{index+1}.png'))
report={'date':'2026-11-03','pdf_sha256':hashlib.sha256((ROOT/'docs/CoralC5-Motor_Schematic.pdf').read_bytes()).hexdigest(),'pages':len(doc),'wire_text_intersections':len(findings),'text_text_overlaps':len(text_overlaps),'ignored_identical_pdf_search_spans':duplicate_search_spans,'findings':findings,'text_overlaps':text_overlaps}
(ROOT/'docs/visual_check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
raise SystemExit(1 if findings or text_overlaps else 0)
