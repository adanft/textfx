.pragma library

function colorWithOpacity(hex, opacity) {
    const raw = String(hex || "000000ff").replace("#", "");
    const r = parseInt(raw.slice(0, 2), 16) || 0;
    const g = parseInt(raw.slice(2, 4), 16) || 0;
    const b = parseInt(raw.slice(4, 6), 16) || 0;
    const a = raw.length >= 8 ? (parseInt(raw.slice(6, 8), 16) || 0) / 255 : 1;
    return Qt.rgba(r / 255, g / 255, b / 255, a * Math.max(0, Math.min(1, opacity)));
}
