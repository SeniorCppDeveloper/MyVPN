package com.nullvpn.app

import android.graphics.Paint as AP
import android.graphics.Typeface
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.RoundRect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.graphics.drawscope.withTransform
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb

const val W = 520f
const val H = 720f
const val TITLE_H = 44f

val WHITE = Color(255, 255, 255, 255)

private val tp = AP(AP.ANTI_ALIAS_FLAG)
private val BoldTf: Typeface = Typeface.create("sans-serif-medium", Typeface.BOLD)
private val RegTf: Typeface = Typeface.create("sans-serif", Typeface.NORMAL)

fun A(a: Int, r: Int, g: Int, b: Int): Color = Color(r, g, b, a.coerceIn(0, 255))
fun cl(t: Float): Float = t.coerceIn(0f, 1f)
fun mix(a: Color, b: Color, t: Float): Color = lerp(a, b, cl(t))
fun withA(c: Color, a: Float): Color = c.copy(alpha = cl(a))
fun fade(c: Color, k: Float): Color = c.copy(alpha = c.alpha * cl(k))
fun ease(t: Float): Float {
    val u = 1f - cl(t)
    return 1f - u * u * u
}

fun textW(s: String, size: Float, bold: Boolean = true): Float {
    tp.typeface = if (bold) BoldTf else RegTf
    tp.textSize = size
    return tp.measureText(s)
}

fun ell(s: String, size: Float, maxW: Float): String {
    if (textW(s, size) <= maxW) return s
    var t = s
    while (t.isNotEmpty() && textW(t + "…", size) > maxW) t = t.dropLast(1)
    return t + "…"
}

fun DrawScope.txt(s: String, x: Float, y: Float, w: Float, h: Float, size: Float, color: Color, align: Int = 1, bold: Boolean = true) {
    if (s.isEmpty()) return
    tp.typeface = if (bold) BoldTf else RegTf
    tp.textSize = size
    tp.color = color.toArgb()
    val fm = tp.fontMetrics
    val base = y + h / 2f - (fm.ascent + fm.descent) / 2f
    val tx: Float
    when (align) {
        0 -> {
            tp.textAlign = AP.Align.LEFT
            tx = x
        }
        1 -> {
            tp.textAlign = AP.Align.CENTER
            tx = x + w / 2f
        }
        else -> {
            tp.textAlign = AP.Align.RIGHT
            tx = x + w
        }
    }
    drawContext.canvas.nativeCanvas.drawText(s, tx, base, tp)
}

fun rrPath(x: Float, y: Float, w: Float, h: Float, r: Float): Path =
    Path().apply { addRoundRect(RoundRect(x, y, x + w, y + h, r, r)) }

fun DrawScope.rr(x: Float, y: Float, w: Float, h: Float, r: Float, color: Color) =
    drawRoundRect(color, Offset(x, y), Size(w, h), CornerRadius(r, r))

fun DrawScope.rrB(x: Float, y: Float, w: Float, h: Float, r: Float, brush: Brush) =
    drawRoundRect(brush, Offset(x, y), Size(w, h), CornerRadius(r, r))

fun DrawScope.rrS(x: Float, y: Float, w: Float, h: Float, r: Float, color: Color, sw: Float = 1f) =
    drawRoundRect(color, Offset(x, y), Size(w, h), CornerRadius(r, r), style = Stroke(sw))

fun vGrad(c1: Color, c2: Color, y0: Float, y1: Float): Brush = Brush.verticalGradient(listOf(c1, c2), startY = y0, endY = y1)
fun hGrad(c1: Color, c2: Color, x0: Float, x1: Float): Brush = Brush.horizontalGradient(listOf(c1, c2), startX = x0, endX = x1)
fun dGrad(c1: Color, c2: Color, x: Float, y: Float, w: Float, h: Float): Brush =
    Brush.linearGradient(listOf(c1, c2), start = Offset(x, y), end = Offset(x + w, y + h))

fun DrawScope.card(x: Float, y: Float, w: Float, h: Float, r: Float, c1: Color, c2: Color, border: Color, bw: Float = 1f) {
    rrB(x, y, w, h, r, vGrad(c1, c2, y, y + h))
    rrS(x, y, w, h, r, border, bw)
}

fun DrawScope.glowRR(x: Float, y: Float, w: Float, h: Float, r: Float, c: Color, k: Float) {
    if (k <= 0.01f) return
    for (i in 2 downTo 1) rrS(x, y, w, h, r, withA(c, k * 0.24f / i), 2f + i * 3f)
}

fun DrawScope.shimmer(clip: Path, y: Float, h: Float, sx: Float, band: Float, a: Float) {
    clipPath(clip) {
        drawRect(
            Brush.horizontalGradient(listOf(Color.Transparent, WHITE.copy(alpha = a), Color.Transparent), startX = sx, endX = sx + band * 2f),
            Offset(sx, y), Size(band * 2f, h)
        )
    }
}

fun DrawScope.blob(cx: Float, cy: Float, rx: Float, ry: Float, c: Color) {
    withTransform({ scale(1f, ry / rx, Offset(cx, cy)) }) {
        drawCircle(Brush.radialGradient(listOf(c, c.copy(alpha = 0f)), Offset(cx, cy), rx), rx, Offset(cx, cy))
    }
}

private fun st(w: Float) = Stroke(w, cap = StrokeCap.Round, join = StrokeJoin.Round)

fun DrawScope.line(c: Color, x1: Float, y1: Float, x2: Float, y2: Float, w: Float) =
    drawLine(c, Offset(x1, y1), Offset(x2, y2), w, StrokeCap.Round)

fun DrawScope.poly(c: Color, w: Float, closed: Boolean, vararg p: Float) {
    val path = Path()
    path.moveTo(p[0], p[1])
    var i = 2
    while (i < p.size) {
        path.lineTo(p[i], p[i + 1])
        i += 2
    }
    if (closed) path.close()
    drawPath(path, c, style = st(w))
}

fun DrawScope.iconCrown(cx: Float, cy: Float, c: Color) =
    poly(c, 1.7f, true, cx - 8f, cy + 5f, cx - 8f, cy - 4f, cx - 3f, cy + 1f, cx, cy - 6f, cx + 3f, cy + 1f, cx + 8f, cy - 4f, cx + 8f, cy + 5f)

fun DrawScope.iconLogout(cx: Float, cy: Float, c: Color) {
    poly(c, 1.8f, false, cx - 1f, cy - 7f, cx - 7f, cy - 7f, cx - 7f, cy + 7f, cx - 1f, cy + 7f)
    line(c, cx - 3f, cy, cx + 8f, cy, 1.8f)
    poly(c, 1.8f, false, cx + 4f, cy - 4f, cx + 8f, cy, cx + 4f, cy + 4f)
}

fun DrawScope.iconUser(cx: Float, cy: Float, c: Color) {
    drawOval(c, Offset(cx - 3.5f, cy - 8f), Size(7f, 7f), style = st(1.7f))
    drawArc(c, 180f, 180f, false, Offset(cx - 7f, cy + 1f), Size(14f, 12f), style = st(1.7f))
}

fun DrawScope.iconLock(cx: Float, cy: Float, c: Color) {
    drawArc(c, 180f, 180f, false, Offset(cx - 4f, cy - 8f), Size(8f, 12f), style = st(1.7f))
    rrS(cx - 6f, cy - 1f, 12f, 9f, 2f, c, 1.7f)
}

fun DrawScope.iconCheck(cx: Float, cy: Float, c: Color, w: Float = 1.8f) =
    poly(c, w, false, cx - 4f, cy, cx - 1f, cy + 3f, cx + 4f, cy - 3f)

fun DrawScope.iconArrow(cx: Float, cy: Float, up: Boolean, c: Color) {
    val s = if (up) -1f else 1f
    line(c, cx, cy - 4f * s, cx, cy + 4f * s, 1.6f)
    poly(c, 1.6f, false, cx - 3f, cy + s, cx, cy + 4f * s, cx + 3f, cy + s)
}

fun DrawScope.iconKey(cx: Float, cy: Float, c: Color) {
    drawOval(c, Offset(cx - 8f, cy - 4f), Size(8f, 8f), style = st(1.7f))
    line(c, cx, cy, cx + 8f, cy, 1.7f)
    line(c, cx + 5f, cy, cx + 5f, cy + 3f, 1.7f)
    line(c, cx + 8f, cy, cx + 8f, cy + 3f, 1.7f)
}

fun DrawScope.drawLogo(x: Float, y: Float, sz: Float) {
    rrB(x, y, sz, sz, sz * 0.3f, dGrad(A(255, 190, 120, 255), A(255, 100, 45, 205), x, y, sz, sz))
    txt("N", x, y, sz, sz, sz * 0.62f, WHITE)
}

fun DrawScope.avatarDisc(a: AppState, cx: Float, cy: Float, r: Float) {
    val img = a.avatar ?: return
    val clip = Path().apply { addOval(Rect(cx - r, cy - r, cx + r, cy + r)) }
    clipPath(clip) {
        withTransform({
            translate(cx - r, cy - r)
            scale(2f * r / img.width, 2f * r / img.height, Offset.Zero)
        }) { drawImage(img) }
    }
}

fun curvePath(p: List<Offset>, tension: Float): Path {
    val path = Path()
    path.moveTo(p[0].x, p[0].y)
    val k = tension / 3f
    for (i in 0 until p.size - 1) {
        val p0 = if (i > 0) p[i - 1] else p[i]
        val p1 = p[i]
        val p2 = p[i + 1]
        val p3 = if (i + 2 < p.size) p[i + 2] else p[i + 1]
        path.cubicTo(
            p1.x + (p2.x - p0.x) * k, p1.y + (p2.y - p0.y) * k,
            p2.x - (p3.x - p1.x) * k, p2.y - (p3.y - p1.y) * k,
            p2.x, p2.y
        )
    }
    return path
}

fun DrawScope.reveal(a: AppState, idx: Int, x: Float, y: Float, w: Float, h: Float, block: DrawScope.() -> Unit) {
    val p = a.revealP(idx)
    withTransform({ translate(0f, (1f - p) * 10f) }) { block() }
    if (p < 0.999f) drawRect(A(((1f - p) * 255f).toInt(), 9, 6, 20), Offset(x - 4f, y), Size(w + 8f, h + 10f))
}