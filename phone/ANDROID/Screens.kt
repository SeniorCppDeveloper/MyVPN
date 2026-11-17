package com.nullvpn.app

import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.graphics.drawscope.withTransform
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sin

fun DrawScope.paintAll(a: AppState) {
    drawRect(vGrad(A(255, 12, 8, 24), A(255, 2, 1, 6), 0f, size.height))
    withTransform({
        translate(a.ox, a.oy)
        scale(a.s, a.s, Offset.Zero)
    }) { paintGlows(a) }
    for (p in a.particles) {
        val tw = 0.5f + 0.5f * sin(a.time * 1.4f + p.phase)
        val pa = 110f * p.alpha * (0.3f + 0.7f * tw) * (0.55f + 0.45f * a.conn)
        drawCircle(A(pa.toInt(), 200, 150, 255), p.r * a.s, Offset(p.x * size.width, p.y * size.height))
    }
    withTransform({
        translate(a.ox, a.oy)
        scale(a.s, a.s, Offset.Zero)
    }) {
        when (a.screen) {
            Screen.LOGIN, Screen.REGISTER -> paintAuth(a)
            Screen.MAIN -> paintMain(a)
            Screen.SUB -> paintSub(a)
        }
    }
}

fun DrawScope.paintGlows(a: AppState) {
    val t = a.time
    val conn = a.conn
    val drift = sin(t * 0.35f) * 14f
    val L = a.L
    when (a.screen) {
        Screen.MAIN -> {
            blob(L.pcx + drift, L.pcy, 230f, 230f, A((26f + 44f * conn).toInt(), 130, 60, 230))
            if (conn > 0.02f) blob(L.pcx - drift, L.pcy, 170f, 170f, A((40f * conn).toInt(), 170, 90, 255))
        }
        Screen.SUB -> blob(W / 2f - drift, L.statusCard.t + 40f, 260f, 150f, if (a.isPremium()) A(40, 225, 160, 255) else A(24, 150, 90, 255))
        else -> blob(W / 2f + drift, L.logo.t + 40f, 260f, 200f, A(34, 130, 60, 230))
    }
    blob(W / 2f, a.areaH - 40f, 240f, 120f, A(22, 100, 50, 200))
}

fun DrawScope.gradButton(rc: Rc, r: Float, hv: Float, pressed: Boolean, ready: Boolean, label: String, size: Float, t: Float) {
    val pr = if (pressed) 1f else 0f
    val x = rc.l + pr * 2f
    val y = rc.t + pr * 1.5f
    val w = rc.w - pr * 4f
    val h = rc.h - pr * 3f
    if (ready) {
        glowRR(x, y, w, h, r, A(255, 190, 100, 255), hv)
        rrB(
            x, y, w, h, r,
            dGrad(
                mix(A(255, 118, 52, 214), A(255, 150, 82, 244), hv),
                mix(A(255, 172, 80, 248), A(255, 214, 118, 255), hv), x, y, w, h
            )
        )
        val sp = (t * 0.4f) % 1.8f - 0.4f
        shimmer(rrPath(x, y, w, h, r), y, h, x + sp * w, 40f, 0.2f)
        rrS(x, y, w, h, r, A(70, 255, 255, 255))
    } else {
        rr(x, y, w, h, r, A(255, 34, 26, 54))
    }
    txt(label, x, y, w, h, size, if (ready) WHITE else A(255, 105, 88, 140))
}

fun DrawScope.fieldBox(r: Rc, f: Float, isErr: Boolean, shakeX: Float, icon: Int, placeholder: String, empty: Boolean) {
    withTransform({ translate(shakeX, 0f) }) {
        if (f > 0.02f) for (i in 2 downTo 1) rrS(r.l, r.t, r.w, r.h, 16f, A((f * 26f / i).toInt(), 160, 90, 255), 2f + i * 3f)
        rr(r.l, r.t, r.w, r.h, 16f, mix(A(255, 17, 12, 30), A(255, 24, 16, 44), f))
        rrS(
            r.l, r.t, r.w, r.h, 16f,
            if (isErr) A(255, 235, 80, 120) else mix(A(255, 56, 42, 92), A(255, 150, 80, 235), f),
            1f + 0.6f * f
        )
        val icc = mix(A(255, 110, 88, 160), A(255, 190, 130, 255), f)
        when (icon) {
            0 -> iconUser(r.l + 26f, r.t + r.h / 2f, icc)
            1 -> iconLock(r.l + 26f, r.t + r.h / 2f, icc)
            else -> iconKey(r.l + 26f, r.t + r.h / 2f, icc)
        }
        if (empty) txt(placeholder, r.l + 50f + 4f * f, r.t, r.w - 50f, r.h, 16f, A(255, 100, 80, 140), 0, false)
    }
}

fun DrawScope.paintAuth(a: AppState) {
    val t = a.time
    val L = a.L
    val isReg = a.screen == Screen.REGISTER

    reveal(a, 0, L.logo.l, L.logo.t, L.logo.w, L.logo.h) {
        val ls = 76f
        val lx = (W - ls) / 2f
        val ly = L.logo.t + 16f
        for (i in 3 downTo 1) {
            rr(
                lx - i * 5f, ly - i * 5f, ls + i * 10f, ls + i * 10f, ls * 0.3f + i * 5f,
                A((16f + 5f * (3 - i) + 6f * sin(t * 1.6f)).toInt(), 160, 90, 255)
            )
        }
        drawLogo(lx, ly + sin(t * 1.3f) * 2f, ls)
    }

    reveal(a, 1, L.titleBlock.l, L.titleBlock.t, L.titleBlock.w, L.titleBlock.h) {
        val b = L.titleBlock
        txt(if (isReg) "СОЗДАТЬ АККАУНТ" else "ВХОД В АККАУНТ", b.l, b.t, b.w, 40f, 26f, WHITE)
        txt(
            if (isReg) "Логин от 3 символов, пароль от 6" else "Войдите, чтобы подключиться",
            b.l, b.t + 42f, b.w, 24f, 14f, A(255, 130, 108, 175), 1, false
        )
    }

    val err = a.loginError.isNotEmpty() && a.shake > 0.01f
    val shakeX = a.shakeX(1)
    reveal(a, 2, L.fieldUser.l, L.fieldUser.t, L.fieldUser.w, L.fieldUser.h) { fieldBox(L.fieldUser, a.focus[0], err, shakeX, 0, "Логин", a.inputUser.isEmpty()) }
    reveal(a, 3, L.fieldPass.l, L.fieldPass.t, L.fieldPass.w, L.fieldPass.h) { fieldBox(L.fieldPass, a.focus[1], err, shakeX, 1, "Пароль", a.inputPass.isEmpty()) }

    reveal(a, 4, L.errLine.l, L.errLine.t, L.loginBtn.r - L.errLine.l, L.loginBtn.b - L.errLine.t) {
        if (a.loginError.isNotEmpty()) txt(a.loginError, L.errLine.l, L.errLine.t, L.errLine.w, L.errLine.h, 12f, A(255, 255, 110, 130))
        val ready = a.inputUser.isNotEmpty() && a.inputPass.isNotEmpty()
        gradButton(
            L.loginBtn, 18f, a.hvOf(Hit.LOGIN), a.down == Hit.LOGIN && a.hit == Hit.LOGIN, ready,
            if (isReg) "ЗАРЕГИСТРИРОВАТЬСЯ" else "ВОЙТИ", 17f, t
        )
    }

    reveal(a, 5, L.switchBtn.l, L.switchBtn.t, L.switchBtn.w, L.switchBtn.h) {
        val b = L.switchBtn
        txt(
            if (isReg) "Уже есть аккаунт? Войти" else "Нет аккаунта? Создать",
            b.l, b.t, b.w, b.h, 15f, mix(A(255, 160, 130, 215), WHITE, a.hvOf(Hit.SWITCH)), 1, false
        )
    }
}

fun fmtSpeed(bps: Float): Pair<String, String> =
    if (bps >= 1048576f) Pair(String.format("%.1f", bps / 1048576f), "МБ/с") else Pair(String.format("%.0f", bps / 1024f), "КБ/с")

fun DrawScope.statCard(rc: Rc, label: String, value: String, unit: String, valueColor: Color, arrow: Int) {
    card(rc.l, rc.t, rc.w, rc.h, 16f, A(255, 20, 14, 36), A(255, 9, 7, 16), A(255, 52, 38, 84))
    var lx = rc.l + 12f
    if (arrow >= 0) {
        iconArrow(rc.l + 17f, rc.t + 17f, arrow == 1, if (arrow == 1) A(255, 200, 140, 255) else A(255, 120, 230, 160))
        lx = rc.l + 28f
    }
    txt(label, lx, rc.t + 8f, rc.w - 16f, 18f, 10f, A(255, 150, 120, 200), 0)
    txt(value, rc.l, rc.t + 28f, rc.w, 28f, 22f, valueColor)
    txt(unit, rc.l, rc.t + 56f, rc.w, 14f, 10f, A(255, 130, 108, 175), 1, false)
}

fun DrawScope.paintMain(a: AppState) {
    val t = a.time
    val conn = a.conn
    val prem = a.isPremium()
    val L = a.L
    val acc = a.acc
    val now = System.currentTimeMillis()

    reveal(a, 0, L.topBar.l, L.topBar.t, L.topBar.w, L.topBar.h) {
        val tb = L.topBar
        drawLogo(tb.l, tb.t + 16f, 28f)
        txt("NULL VPN", tb.l + 36f, tb.t, 110f, tb.h, 16f, A(255, 215, 190, 255), 0)
        val ab = L.avatarBtn
        val acx = (ab.l + ab.r) / 2f
        val acy = (ab.t + ab.b) / 2f
        val hv = a.hvOf(Hit.AVATAR)
        if (a.avatar != null) {
            avatarDisc(a, acx, acy, 20f)
        } else {
            drawCircle(dGrad(A(255, 190, 120, 255), A(255, 100, 45, 205), acx - 20f, acy - 20f, 40f, 40f), 20f, Offset(acx, acy))
            txt((acc?.user?.take(1) ?: "?").uppercase(), acx - 20f, acy - 20f, 40f, 40f, 17f, WHITE)
        }
        drawCircle(
            if (prem) A(255, 235, 190, 255) else mix(A(255, 90, 68, 140), A(255, 190, 130, 255), hv),
            22f, Offset(acx, acy), style = Stroke(2f)
        )
        val nx = tb.l + 150f
        val nw = ab.l - 10f - nx
        txt(ell(acc?.user ?: "", 15f, nw), nx, tb.t + 10f, nw, 22f, 15f, WHITE, 2)
        txt(
            if (prem) "PREMIUM • " + a.daysLeftSub() + " дн." else "Нет подписки",
            nx, tb.t + 32f, nw, 18f, 11f, if (prem) A(255, 235, 190, 255) else A(255, 150, 120, 200), 2
        )
    }

    val cx = L.pcx
    val cy = L.pcy
    val r0 = L.pr
    val hover = a.hvOf(Hit.CONNECT)
    val r = r0 * (1f + hover * 0.03f - a.press * 0.05f)

    reveal(a, 1, cx - r0 - 14f, cy - r0 - 14f, (r0 + 14f) * 2f, (r0 + 14f) * 2f) {
        if (conn > 0.02f) {
            for (k in 0 until 3) {
                val ph = (t * 0.45f + k / 3f) % 1f
                drawCircle(
                    A(((1f - ph) * (1f - ph) * 110f * conn).toInt(), 200, 120, 255),
                    r + 6f + ph * 34f, Offset(cx, cy), style = Stroke(2f)
                )
            }
        }
        if (a.rip > 0f) {
            drawCircle(A(((1f - a.rip) * 160f).toInt(), 235, 190, 255), r + 6f + ease(a.rip) * 36f, Offset(cx, cy), style = Stroke(2f))
        }
        drawCircle(mix(A(255, 70, 50, 100), A(255, 190, 110, 255), conn), r + 8f, Offset(cx, cy), style = Stroke(3f))
        val c1 = mix(mix(A(255, 88, 36, 165), A(255, 150, 64, 238), conn), A(255, 175, 120, 255), hover * 0.25f)
        val c2 = mix(mix(A(255, 128, 62, 208), A(255, 222, 98, 255), conn), A(255, 210, 150, 255), hover * 0.25f)
        drawCircle(dGrad(c1, c2, cx - r, cy - r, r * 2f, r * 2f), r, Offset(cx, cy))
        val clip = Path().apply { addOval(Rect(cx - r, cy - r, cx + r, cy + r)) }
        val sp = (t * (if (a.vstate == VState.CONNECTING) 1.1f else 0.3f)) % 1.8f - 0.4f
        shimmer(clip, cy - r, r * 2f, cx - r + sp * r * 2f, r * 0.45f, 0.2f)
        drawCircle(A(40, 255, 255, 255), r - 8f, Offset(cx, cy), style = Stroke(1.2f))
        if (a.vstate != VState.OFF) {
            drawArc(
                A(255, 235, 180, 255),
                (t * (if (a.vstate == VState.CONNECTING) 420f else 150f)) % 360f, 80f, false,
                Offset(cx - r - 8f, cy - r - 8f), Size((r + 8f) * 2f, (r + 8f) * 2f),
                style = Stroke(3f, cap = StrokeCap.Round)
            )
        }

        val icy = cy - r * 0.16f
        val ir = r * 0.28f
        val sw = max(4f, r * 0.07f)
        if (a.vstate == VState.CONNECTING) {
            drawArc(
                WHITE, (t * 420f) % 360f, 260f, false, Offset(cx - ir, icy - ir), Size(ir * 2f, ir * 2f),
                style = Stroke(sw, cap = StrokeCap.Round)
            )
        } else if (!prem && a.vstate == VState.OFF) {
            withTransform({
                translate(cx, icy)
                scale(r / 22f, r / 22f, Offset.Zero)
            }) { iconCrown(0f, 0f, WHITE) }
        } else {
            drawArc(
                WHITE, 300f, 300f, false, Offset(cx - ir, icy - ir), Size(ir * 2f, ir * 2f),
                style = Stroke(sw, cap = StrokeCap.Round)
            )
            line(WHITE, cx, icy - ir * 1.15f, cx, icy - ir * 0.05f, sw)
        }

        val ty = cy + r * 0.52f
        if (!prem && a.vstate == VState.OFF) {
            txt("ОФОРМИТЬ", cx - r, ty - 8f, r * 2f, 16f, 12f, WHITE)
            txt("ПОДПИСКУ", cx - r, ty + 8f, r * 2f, 16f, 12f, WHITE)
        } else if (a.vstate == VState.ON) {
            val s = ((now - a.connectedAt) / 1000L).toInt()
            txt(String.format("%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60), cx - r, ty - 10f, r * 2f, 24f, 20f, WHITE)
        } else if (a.vstate == VState.CONNECTING) {
            txt("ПОДКЛЮЧЕНИЕ" + ".".repeat(((t * 3f).toInt()) % 4), cx - r, ty - 8f, r * 2f, 16f, 13f, WHITE)
        } else {
            txt("ПОДКЛЮЧИТЬ", cx - r, ty - 8f, r * 2f, 16f, 14f, WHITE)
        }
    }

    val showErr = a.errMsg != null && now < a.errUntil
    val noReply = a.vstate == VState.ON && a.failStreak >= 3
    val st = if (showErr) a.errMsg!!
    else when (a.vstate) {
        VState.ON -> if (noReply) "НЕТ ОТВЕТА" else "ЗАЩИЩЕНО"
        VState.CONNECTING -> "ПОДКЛЮЧЕНИЕ"
        else -> if (prem) "НЕ ПОДКЛЮЧЕНО" else "НУЖНА ПОДПИСКА"
    }
    val tc = if (showErr) A(255, 255, 120, 145)
    else when (a.vstate) {
        VState.ON -> if (noReply) A(255, 240, 200, 90) else A(255, 200, 255, 220)
        VState.CONNECTING -> A(255, 170, 140, 215)
        else -> if (prem) A(255, 170, 140, 215) else A(255, 240, 200, 110)
    }
    reveal(a, 2, L.statusPill.l, L.statusPill.t, L.statusPill.w, L.statusPill.h) {
        val sp = L.statusPill
        val shown = ell(st, 14f, sp.w - 50f)
        val pw = min(sp.w, textW(shown, 14f) + 44f)
        val px = (W - pw) / 2f
        rr(px, sp.t, pw, sp.h, sp.h / 2f, tc.copy(alpha = 0.13f))
        rrS(px, sp.t, pw, sp.h, sp.h / 2f, tc.copy(alpha = 0.35f))
        val dotA = when (a.vstate) {
            VState.CONNECTING -> 0.5f + 0.5f * sin(t * 8f)
            VState.ON -> 0.75f + 0.25f * sin(t * 2.5f)
            else -> 1f
        }
        drawCircle(tc.copy(alpha = dotA), 3.5f, Offset(px + 16f, sp.t + sp.h / 2f))
        txt(shown, px + 22f, sp.t, pw - 22f, sp.h, 14f, tc)
    }

    reveal(a, 3, L.stats[0].l, L.stats[0].t, L.stats[2].r - L.stats[0].l, L.stats[0].h) {
        val on = a.vstate == VState.ON
        val d = fmtSpeed(a.downSpeed)
        val u = fmtSpeed(a.upSpeed)
        statCard(L.stats[0], "СКАЧИВАНИЕ", if (on) d.first else "—", if (on) d.second else "", A(255, 215, 200, 245), 0)
        statCard(L.stats[1], "ОТДАЧА", if (on) u.first else "—", if (on) u.second else "", A(255, 215, 200, 245), 1)
        val p = a.ping
        val pc = if (p < 80) A(255, 120, 230, 160) else if (p < 150) A(255, 240, 200, 90) else A(255, 240, 90, 120)
        statCard(
            L.stats[2], "ПИНГ", if (on && p > 0) p.toString() else "—", if (on && p > 0) "мс" else "",
            if (on && p > 0) mix(A(255, 150, 120, 200), pc, a.pingFade) else A(255, 110, 90, 150), -1
        )
    }

    reveal(a, 4, L.graph.l, L.graph.t, L.graph.w, L.graph.h) {
        val g = L.graph
        card(g.l, g.t, g.w, g.h, 18f, A(255, 20, 14, 36), A(255, 9, 7, 16), A(255, 52, 38, 84))
        txt("ЗАДЕРЖКА", g.l + 14f, g.t + 8f, 120f, 18f, 10f, A(255, 150, 120, 200), 0)
        val gl = g.l + 14f
        val gr = g.r - 14f
        val gt = g.t + 40f
        val gb = g.b - 16f
        for (i in 0 until 3) {
            val y = gt + (gb - gt) * i / 2f
            drawLine(A(255, 24, 16, 38), Offset(gl, y), Offset(gr, y), 1f)
        }
        val n = a.pings.size
        if (a.vstate == VState.ON && n >= 2) {
            val step = (gr - gl) / 39f
            val pts = ArrayList<Offset>()
            for (i in 0 until n) pts.add(Offset(gr - (n - 1 - i) * step, gb - cl(a.pings[i] / 220f) * (gb - gt)))
            val lp = curvePath(pts, 0.4f)
            val fp = Path()
            fp.addPath(lp)
            fp.lineTo(pts[n - 1].x, gb)
            fp.lineTo(pts[0].x, gb)
            fp.close()
            drawPath(fp, vGrad(A(90, 170, 90, 255), A(0, 170, 90, 255), gt, gb))
            drawPath(lp, A(255, 195, 125, 255), style = Stroke(2.2f, join = StrokeJoin.Round))
            val last = pts[n - 1]
            val pr = 3f + 2f * (0.5f + 0.5f * sin(t * 4f))
            drawCircle(A(60, 200, 130, 255), pr + 3f, last)
            drawCircle(WHITE, 3f, last)
        } else {
            txt("Подключитесь, чтобы увидеть график", g.l, gt, g.w, gb - gt, 12f, A(255, 100, 80, 140), 1, false)
        }
    }

    reveal(a, 5, L.subBtn.l, L.subBtn.t, L.outBtn.r - L.subBtn.l, L.subBtn.h) {
        gradButton(L.subBtn, 20f, a.hvOf(Hit.GO_SUB), a.down == Hit.GO_SUB && a.hit == Hit.GO_SUB, true, "Подписка", 17f, t)
        iconCrown(L.subBtn.l + 30f, L.subBtn.t + L.subBtn.h / 2f, WHITE)

        val o = L.outBtn
        val armed = now < a.logoutArmedUntil
        val hv = a.hvOf(Hit.DO_LOGOUT)
        val pr = if (a.down == Hit.DO_LOGOUT && a.hit == Hit.DO_LOGOUT) 1f else 0f
        val ox = o.l + pr * 2f
        val oy = o.t + pr * 1.5f
        val ow = o.w - pr * 4f
        val oh = o.h - pr * 3f
        val base = if (armed) A(255, 235, 60, 110) else mix(A(255, 255, 125, 150), A(255, 255, 160, 180), hv)
        rr(ox, oy, ow, oh, 20f, base.copy(alpha = if (armed) 0.28f else 0.10f + 0.10f * hv))
        rrS(ox, oy, ow, oh, 20f, base.copy(alpha = 0.8f), 1.6f)
        iconLogout(ox + 30f, oy + oh / 2f, base)
        txt(if (armed) "Нажмите ещё раз" else "Выйти", ox + 44f, oy, ow - 44f, oh, if (armed) 14f else 17f, base)
    }
}

fun DrawScope.paintSub(a: AppState) {
    val t = a.time
    val prem = a.isPremium()
    val acc = a.acc
    val L = a.L

    reveal(a, 0, L.header.l, L.header.t, L.header.w, L.header.h) {
        val b = L.back
        val hv = a.hvOf(Hit.BACK)
        drawCircle(A((40f * hv).toInt(), 190, 140, 255), 22f, Offset(b.l + 22f, b.t + 22f))
        drawCircle(mix(A(255, 80, 58, 130), A(255, 190, 130, 255), hv), 22f, Offset(b.l + 22f, b.t + 22f), style = Stroke(1.2f))
        val cx = b.l + 22f - hv * 1.5f
        val cy = b.t + 22f
        poly(mix(A(255, 170, 140, 225), WHITE, hv), 2.2f, false, cx + 3.5f, cy - 7f, cx - 3.5f, cy, cx + 3.5f, cy + 7f)
        txt("ПОДПИСКА", L.header.l, L.header.t, L.header.w, L.header.h, 22f, WHITE)
    }

    reveal(a, 1, L.statusCard.l, L.statusCard.t, L.statusCard.w, L.statusCard.h) {
        val c = L.statusCard
        val breathe = 0.5f + 0.5f * sin(t * 1.8f)
        val border = if (prem) mix(A(255, 120, 70, 210), A(255, 225, 160, 255), breathe)
        else mix(A(255, 52, 38, 84), A(255, 92, 66, 150), breathe)
        if (prem) glowRR(c.l, c.t, c.w, c.h, 20f, A(255, 220, 150, 255), 0.5f + 0.5f * breathe)
        card(
            c.l, c.t, c.w, c.h, 20f,
            if (prem) A(255, 36, 24, 66) else A(255, 20, 14, 36),
            if (prem) A(255, 16, 10, 32) else A(255, 9, 7, 16),
            border, if (prem) 1.6f else 1f
        )
        val sp = (t * (if (prem) 0.2f else 0.12f)) % 1.7f - 0.35f
        shimmer(rrPath(c.l, c.t, c.w, c.h, 20f), c.t, c.h, c.l + sp * c.w, 40f, if (prem) 0.11f else 0.055f)

        val icx = c.l + 46f
        val icy = c.t + c.h / 2f + sin(t * 2f) * 1.5f
        val ir = 28f
        if (prem) {
            for (i in 2 downTo 1) {
                drawCircle(A(((30 / i) * (0.5f + 0.5f * breathe)).toInt(), 220, 150, 255), ir, Offset(icx, icy), style = Stroke(2f + i * 3f))
            }
        }
        drawCircle(
            dGrad(
                if (prem) A(255, 190, 120, 255) else A(255, 40, 32, 60),
                if (prem) A(255, 100, 45, 205) else A(255, 24, 18, 40),
                icx - ir, icy - ir, ir * 2f, ir * 2f
            ),
            ir, Offset(icx, icy)
        )
        drawCircle(if (prem) A(255, 235, 190, 255) else A(255, 70, 54, 110), ir, Offset(icx, icy), style = Stroke(1.5f))
        withTransform({
            translate(icx, icy)
            scale(1.7f, 1.7f, Offset.Zero)
        }) {
            if (prem) iconCrown(0f, 0f, WHITE) else iconLock(0f, 0f, A(255, 130, 108, 180))
        }

        val tx = c.l + 90f
        txt("ТЕКУЩИЙ ПЛАН", tx, c.t + 16f, 150f, 16f, 10f, A(255, 150, 120, 200), 0)
        txt(
            if (prem) "PREMIUM" else "НЕТ ПОДПИСКИ", tx, c.t + 34f, 160f, 32f, 22f,
            if (prem) A(255, 235, 190, 255) else A(255, 200, 180, 235), 0
        )
        val line = if (prem && acc != null) "до " + a.fmtDate(acc.subExpire) else "Выберите тариф или введите ключ"
        txt(line, tx, c.t + 74f, c.r - tx - 10f, 22f, 13f, if (prem) A(255, 215, 200, 245) else A(255, 130, 108, 175), 0, false)

        if (prem) {
            txt(a.daysLeftSub().toString(), c.r - 84f, c.t + 20f, 70f, 36f, 26f, A(255, 235, 190, 255), 2)
            txt("ДНЕЙ ОСТАЛОСЬ", c.r - 120f, c.t + 56f, 106f, 14f, 9f, A(255, 130, 108, 175), 2)
        } else {
            val pw = 84f
            val px = c.r - 14f - pw
            val py = c.t + 16f
            rr(px, py, pw, 24f, 12f, A((26f + 14f * breathe).toInt(), 255, 120, 145))
            rrS(px, py, pw, 24f, 12f, A(110, 255, 120, 145))
            txt("НЕ АКТИВНА", px, py, pw, 24f, 10f, A(255, 255, 140, 160))
        }
    }

    reveal(a, 2, L.trafficCard.l, L.trafficCard.t, L.trafficCard.w, L.trafficCard.h) {
        val c = L.trafficCard
        val pulse = 0.5f + 0.5f * sin(t * 2f)
        card(
            c.l, c.t, c.w, c.h, 18f, A(255, 20, 14, 36), A(255, 9, 7, 16),
            mix(A(255, 52, 38, 84), A(255, 110, 76, 190), if (prem) pulse else 0f)
        )
        val now = nowUnix()
        val el = if (acc != null && acc.periodStart != 0L) now - acc.periodStart else 0L
        val day = if (prem) min(30L, el / 86400L + 1L).toInt() else 0
        val left = if (prem) max(0L, (PERIOD_SEC - el + 86399L) / 86400L).toInt() else 0
        txt("ТРАФИК • БЕЗЛИМИТ", c.l + 16f, c.t + 10f, 160f, 16f, 10f, A(255, 150, 120, 200), 0)
        txt(
            if (prem) "Сброс через $left дн." else "Доступен с подпиской",
            c.r - 176f, c.t + 10f, 160f, 16f, 10f, A(255, 130, 108, 175), 2
        )
        txt(String.format("%.1f ГБ", a.trafficShown), c.l + 16f, c.t + 30f, 150f, 30f, 22f, if (prem) WHITE else A(255, 120, 100, 165), 0)
        if (prem) txt("День $day из 30", c.r - 156f, c.t + 34f, 140f, 22f, 12f, A(255, 190, 170, 235), 2)

        val bx = c.l + 16f
        val by = c.b - 20f
        val bw = c.w - 32f
        val bh = 8f
        rr(bx, by, bw, bh, 4f, A(255, 40, 30, 60))
        val prog = if (prem) cl(a.periodBar) else 0f
        if (prog > 0.01f) {
            val fw = max(bh, bw * prog)
            rrB(bx, by, fw, bh, 4f, hGrad(A(255, 120, 60, 230), A(255, 215, 125, 255), bx, bx + fw))
            val sp = (t * 0.5f) % 1.6f - 0.3f
            shimmer(rrPath(bx, by, fw, bh, 4f), by, bh, bx + sp * fw, 24f, 0.27f)
            drawCircle(A((50f + 30f * sin(t * 3f)).toInt(), 215, 125, 255), 10f, Offset(bx + fw, by + bh / 2f))
        }
        clipPath(rrPath(bx, by, bw, bh, 4f)) {
            for (i in 1 until 30) {
                val tx = bx + bw * i / 30f
                drawLine(A(110, 12, 8, 24), Offset(tx, by), Offset(tx, by + bh), 1f)
            }
        }
    }

    for (i in 0..2) {
        val rc = L.plans[i]
        reveal(a, 3 + i, rc.l, rc.t, rc.w, rc.h) {
            val hv = a.hvOf(Hit.values()[Hit.PLAN0.ordinal + i])
            val sel = a.planSel[i]
            val px = rc.l
            val py = rc.t - hv * 2f - sel * 2f
            val pw = rc.w
            val ph = rc.h
            val pl = PLANS[i]

            glowRR(px, py, pw, ph, 18f, A(255, 180, 110, 255), sel + hv * 0.3f)
            card(
                px, py, pw, ph, 18f,
                mix(A(255, 20, 14, 36), A(255, 42, 28, 76), sel),
                mix(A(255, 9, 7, 16), A(255, 22, 14, 42), sel),
                mix(A(255, 52, 38, 84), A(255, 195, 125, 255), max(sel, hv * 0.5f)),
                1f + sel * 0.8f
            )

            val rx = px + 14f
            val ry = py + 14f
            drawCircle(mix(A(255, 80, 62, 125), A(255, 195, 125, 255), sel), 7f, Offset(rx, ry), style = Stroke(1.4f))
            if (sel > 0.02f) {
                drawCircle(A((sel * 255f).toInt(), 150, 80, 235), 7f * sel, Offset(rx, ry))
                iconCheck(rx, ry, A((sel * 255f).toInt(), 255, 255, 255), 1.5f)
            }

            val pct = (100.0 - 100.0 * pl.price / (pl.months * BASE_PRICE.toDouble())).roundToInt()
            if (pct > 0) {
                val gold = i == 2
                val cc = if (gold) A(255, 235, 190, 255) else A(255, 150, 245, 190)
                rr(px + pw - 40f, py + 8f, 32f, 15f, 7.5f, cc.copy(alpha = 0.16f))
                rrS(px + pw - 40f, py + 8f, 32f, 15f, 7.5f, cc.copy(alpha = 0.55f))
                txt("-$pct%", px + pw - 40f, py + 8f, 32f, 15f, 9f, cc)
            }

            val unit = if (pl.months == 1) "МЕСЯЦ" else if (pl.months < 5) "МЕСЯЦА" else "МЕСЯЦЕВ"
            txt(pl.months.toString(), px, py + 24f, pw, 32f, 26f, WHITE)
            txt(unit, px, py + 56f, pw, 12f, 9f, A(255, 150, 120, 200))
            txt(pl.price.toString() + " ₽", px, py + 74f, pw, 20f, 16f, mix(WHITE, A(255, 235, 190, 255), sel))
            txt((pl.price.toDouble() / pl.months).roundToInt().toString() + " ₽/мес", px, py + 96f, pw, 12f, 9f, A(255, 130, 108, 175), 1, false)
        }
    }

    reveal(a, 6, L.buy.l, L.buy.t, L.buy.w, L.buy.h) {
        gradButton(
            L.buy, 20f, a.hvOf(Hit.BUY), a.down == Hit.BUY && a.hit == Hit.BUY, true,
            (if (prem) "ПРОДЛИТЬ" else "КУПИТЬ") + "  •  " + a.priceShown.roundToInt() + " ₽", 17f, t
        )
    }

    reveal(a, 7, L.keyLabel.l, L.keyLabel.t, L.activate.r - L.keyLabel.l, L.keyMsg.b - L.keyLabel.t) {
        txt("ИЛИ ВВЕДИТЕ КЛЮЧ АКТИВАЦИИ", L.keyLabel.l, L.keyLabel.t, L.keyLabel.w, L.keyLabel.h, 10f, A(255, 150, 120, 200), 0)
        val kerr = a.keyMsgT > 0f && !a.keyMsgOk && a.keyShake > 0.01f
        fieldBox(L.keyField, a.focus[2], kerr, a.shakeX(2), 2, "XXXXX-XXXXX-XXXXX-XXXXX", a.inputKey.isEmpty())
        gradButton(
            L.activate, 16f, a.hvOf(Hit.ACTIVATE), a.down == Hit.ACTIVATE && a.hit == Hit.ACTIVATE,
            a.inputKey.isNotEmpty(), "АКТИВИРОВАТЬ", 15f, t
        )
        if (a.keyMsgT > 0f) {
            val mc = if (a.keyMsgOk) A(255, 130, 240, 170) else A(255, 255, 110, 130)
            txt(a.keyMsg, L.keyMsg.l, L.keyMsg.t, L.keyMsg.w, L.keyMsg.h, 12f, withA(mc, a.keyMsgT))
        }
    }
}