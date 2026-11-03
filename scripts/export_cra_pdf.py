"""Export docs/cra/*.md to a readable PDF."""
from __future__ import annotations

import re
from pathlib import Path

from reportlab.lib.colors import HexColor, white
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import cm
from reportlab.platypus import (
    HRFlowable,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)

ROOT = Path(__file__).resolve().parents[1] / "docs" / "cra"
OUT = ROOT / "export" / "CRA-dossier-Nesso-N1.pdf"

FILES = [
    "README.md",
    "01-product-en-toepassing.md",
    "02-architectuur-en-interfaces.md",
    "03-componenteninventaris.md",
    "04-risicoanalyse-threat-model.md",
    "05-annex-I-controlematrix.md",
    "06-secure-development.md",
    "07-update-en-releasebeleid.md",
    "08-vulnerability-handling.md",
    "09-gebruikersinformatie.md",
    "10-change-control.md",
    "11-technische-documentatie-checklist.md",
    "12-hardware-certificaten-arduino.md",
    "13-systeemgrenzen-fabrikantrollen.md",
]


def build_styles():
    styles = getSampleStyleSheet()
    styles.add(
        ParagraphStyle(
            name="CoverTitle",
            fontName="Helvetica-Bold",
            fontSize=20,
            leading=26,
            alignment=TA_CENTER,
            spaceAfter=12,
            textColor=HexColor("#1a1a1a"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="CoverSub",
            fontName="Helvetica",
            fontSize=12,
            leading=16,
            alignment=TA_CENTER,
            spaceAfter=8,
            textColor=HexColor("#333333"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="H1Doc",
            fontName="Helvetica-Bold",
            fontSize=16,
            leading=20,
            spaceBefore=6,
            spaceAfter=10,
            textColor=HexColor("#111111"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="H2Doc",
            fontName="Helvetica-Bold",
            fontSize=13,
            leading=17,
            spaceBefore=12,
            spaceAfter=6,
            textColor=HexColor("#222222"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="H3Doc",
            fontName="Helvetica-Bold",
            fontSize=11,
            leading=14,
            spaceBefore=8,
            spaceAfter=4,
            textColor=HexColor("#333333"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="BodyDoc",
            fontName="Helvetica",
            fontSize=9.5,
            leading=13,
            spaceAfter=6,
            alignment=TA_JUSTIFY,
        )
    )
    styles.add(
        ParagraphStyle(
            name="CodeDoc",
            fontName="Courier",
            fontSize=8,
            leading=11,
            spaceBefore=2,
            spaceAfter=2,
            leftIndent=8,
            textColor=HexColor("#222222"),
            backColor=HexColor("#f4f4f4"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="BulletDoc",
            fontName="Helvetica",
            fontSize=9.5,
            leading=13,
            leftIndent=14,
            spaceAfter=3,
        )
    )
    styles.add(ParagraphStyle(name="TableCell", fontName="Helvetica", fontSize=8, leading=10))
    styles.add(ParagraphStyle(name="TableHeader", fontName="Helvetica-Bold", fontSize=8, leading=10))
    styles.add(
        ParagraphStyle(
            name="Disclaimer",
            fontName="Helvetica",
            fontSize=9,
            leading=12,
            alignment=TA_CENTER,
            textColor=HexColor("#660000"),
            spaceBefore=8,
            spaceAfter=8,
        )
    )
    return styles


def esc(s: str) -> str:
    s = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    s = re.sub(r"`([^`]+)`", r"<font face='Courier' size='8'>\1</font>", s)
    s = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", s)
    s = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<i>\1</i>", s)
    s = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"<link href='\2' color='blue'>\1</link>", s)
    return s


def parse_table(lines):
    rows = []
    for line in lines:
        line = line.strip()
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if all(c and set(c) <= set("-: ") for c in cells):
            continue
        rows.append(cells)
    return rows


def table_flowable(rows, styles):
    if not rows:
        return None
    ncol = max(len(r) for r in rows)
    norm = []
    for i, r in enumerate(rows):
        r = r + [""] * (ncol - len(r))
        style = styles["TableHeader"] if i == 0 else styles["TableCell"]
        norm.append([Paragraph(esc(c), style) for c in r])
    width = 16.5 * cm
    col_w = [width / ncol] * ncol
    t = Table(norm, colWidths=col_w, repeatRows=1)
    t.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), HexColor("#e8e8e8")),
                ("GRID", (0, 0), (-1, -1), 0.4, HexColor("#aaaaaa")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 4),
                ("RIGHTPADDING", (0, 0), (-1, -1), 4),
                ("TOPPADDING", (0, 0), (-1, -1), 3),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [white, HexColor("#fafafa")]),
            ]
        )
    )
    return t


def md_to_flowables(text: str, styles):
    flow = []
    lines = text.splitlines()
    i = 0
    in_code = False
    code_buf: list[str] = []
    while i < len(lines):
        line = lines[i]
        if line.strip().startswith("```"):
            if not in_code:
                in_code = True
                code_buf = []
            else:
                in_code = False
                for chunk in "\n".join(code_buf).split("\n"):
                    flow.append(Paragraph(esc(chunk) if chunk else "&nbsp;", styles["CodeDoc"]))
            i += 1
            continue
        if in_code:
            code_buf.append(line)
            i += 1
            continue
        if line.startswith("|"):
            table_lines = []
            while i < len(lines) and lines[i].startswith("|"):
                table_lines.append(lines[i])
                i += 1
            tf = table_flowable(parse_table(table_lines), styles)
            if tf:
                flow.append(Spacer(1, 4))
                flow.append(tf)
                flow.append(Spacer(1, 8))
            continue
        if line.startswith("# "):
            flow.append(Paragraph(esc(line[2:].strip()), styles["H1Doc"]))
        elif line.startswith("## "):
            flow.append(Paragraph(esc(line[3:].strip()), styles["H2Doc"]))
        elif line.startswith("### "):
            flow.append(Paragraph(esc(line[4:].strip()), styles["H3Doc"]))
        elif re.match(r"^[-*] ", line):
            flow.append(Paragraph("• " + esc(line[2:].strip()), styles["BulletDoc"]))
        elif re.match(r"^\d+\. ", line):
            flow.append(Paragraph(esc(line.strip()), styles["BulletDoc"]))
        elif line.strip() == "":
            flow.append(Spacer(1, 4))
        elif line.strip() and set(line.strip()) <= set("-"):
            flow.append(
                HRFlowable(
                    width="100%",
                    thickness=0.5,
                    color=HexColor("#cccccc"),
                    spaceBefore=4,
                    spaceAfter=4,
                )
            )
        else:
            flow.append(Paragraph(esc(line.strip()), styles["BodyDoc"]))
        i += 1
    return flow


def on_page(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 8)
    canvas.setFillColor(HexColor("#666666"))
    canvas.drawString(2.2 * cm, 1.2 * cm, "Nesso N1 — CRA technisch dossier (draft)")
    canvas.drawRightString(A4[0] - 2.2 * cm, 1.2 * cm, f"Pagina {doc.page}")
    canvas.restoreState()


def main():
    styles = build_styles()
    OUT.parent.mkdir(parents=True, exist_ok=True)

    story = [
        Spacer(1, 3 * cm),
        Paragraph("Nesso N1 Heatpump Bridge", styles["CoverTitle"]),
        Paragraph("CRA technisch dossier", styles["CoverTitle"]),
        Spacer(1, 0.5 * cm),
        Paragraph("Annex I / VII gerichte conceptdocumentatie", styles["CoverSub"]),
        Paragraph("Versie 0.1-draft — 2026-11-01", styles["CoverSub"]),
        Spacer(1, 1 * cm),
        HRFlowable(width="60%", thickness=1, color=HexColor("#333333"), spaceBefore=4, spaceAfter=12),
        Paragraph(
            "<b>Disclaimer:</b> Dit is conceptuele technische documentatie voor een CRA-dossier. "
            "Geen EU-verklaring van overeenstemming en geen juridische claim dat het product "
            "“CRA-conform” is.",
            styles["Disclaimer"],
        ),
        Paragraph(
            "Bron: docs/cra/ in de projectrepository. Na review kopiëren naar SharePoint "
            "met versie en goedkeurder.",
            styles["CoverSub"],
        ),
        PageBreak(),
    ]

    for idx, name in enumerate(FILES):
        text = (ROOT / name).read_text(encoding="utf-8")
        if idx > 0:
            story.append(PageBreak())
        story.extend(md_to_flowables(text, styles))

    doc = SimpleDocTemplate(
        str(OUT),
        pagesize=A4,
        leftMargin=2.2 * cm,
        rightMargin=2.2 * cm,
        topMargin=1.8 * cm,
        bottomMargin=2.0 * cm,
        title="Nesso N1 CRA technisch dossier",
        author="Nesso N1 project",
    )
    doc.build(story, onFirstPage=on_page, onLaterPages=on_page)
    print(f"Wrote {OUT} ({OUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
