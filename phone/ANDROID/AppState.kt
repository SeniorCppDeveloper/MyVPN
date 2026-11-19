package com.nullvpn.app

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.drawable.Drawable
import android.net.Uri
import android.net.VpnService
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin

enum class Screen { LOGIN, REGISTER, MAIN, SUB, APPS }
enum class Hit { NONE, CONNECT, AVATAR, GO_SUB, DO_LOGOUT, LOGIN, SWITCH, BUY, BACK, PLAN0, PLAN1, PLAN2, ACTIVATE, APPS, APP_TOGGLE, APPS_MASTER, APPLY_SPLIT, SEARCH }
enum class VState { OFF, CONNECTING, ON }

class AppInfo(val label: String, val pkg: String, val icon: ImageBitmap?)

class Plan(val months: Int, val days: Int, val price: Int, val code: String)

val PLANS = listOf(Plan(1, 30, 199, "1m"), Plan(3, 90, 499, "3m"), Plan(12, 365, 1690, "12m"))
const val BASE_PRICE = 199
const val PERIOD_SEC = 30L * 86400L
const val GB_F = 1073741824f
const val APPS_ROW_H = 64f

class Particle(var x: Float, var y: Float, val r: Float, val speed: Float, val phase: Float, val alpha: Float)

class Rc(val l: Float, val t: Float, val r: Float, val b: Float) {
    val w: Float get() = r - l
    val h: Float get() = b - t
    fun has(x: Float, y: Float) = x >= l && x <= r && y >= t && y <= b
}

val ZR = Rc(0f, 0f, 0f, 0f)

fun flow(top: Float, bottom: Float, hs: FloatArray, gmin: Float, gmax: Float): FloatArray {
    val n = hs.size
    val total = hs.sum()
    val g = if (n > 1) ((bottom - top - total) / (n - 1)).coerceIn(gmin, gmax) else 0f
    var y = top
    val out = FloatArray(n)
    for (i in 0 until n) {
        out[i] = y
        y += hs[i] + g
    }
    return out
}

class Layout {
    var logo = ZR
    var titleBlock = ZR
    var fieldUser = ZR
    var fieldPass = ZR
    var errLine = ZR
    var loginBtn = ZR
    var switchBtn = ZR
    var topBar = ZR
    var avatarBtn = ZR
    var pcx = 0f
    var pcy = 0f
    var pr = 80f
    var statusPill = ZR
    var stats = arrayOf(ZR, ZR, ZR)
    var graph = ZR
    var appsBtn = ZR
    var subBtn = ZR
    var outBtn = ZR
    var header = ZR
    var back = ZR
    var statusCard = ZR
    var trafficCard = ZR
    var plans = arrayOf(ZR, ZR, ZR)
    var buy = ZR
    var keyLabel = ZR
    var keyField = ZR
    var activate = ZR
    var keyMsg = ZR
    var kbBottom = 0f
    var appsMaster = ZR
    var appsSearch = ZR
    var appsListTop = 0f
    var appsListBottom = 0f
    var appsApply = ZR
    var appsScrollBar = ZR

    fun compute(sc: Screen, hd: Float, ti: Float, bi: Float) {
        val top = ti + 12f
        val bot = hd - bi - 16f
        val m = 16f
        when (sc) {
            Screen.LOGIN, Screen.REGISTER -> {
                val ys = flow(top, bot, floatArrayOf(140f, 80f, 72f, 72f, 20f, 76f, 52f), 22f, 1000f)
                logo = Rc(W / 2f - 50f, ys[0], W / 2f + 50f, ys[0] + 110f)
                titleBlock = Rc(m, ys[1], W - m, ys[1] + 70f)
                fieldUser = Rc(m, ys[2], W - m, ys[2] + 72f)
                fieldPass = Rc(m, ys[3], W - m, ys[3] + 72f)
                errLine = Rc(m, ys[4], W - m, ys[4] + 20f)
                loginBtn = Rc(m, ys[5], W - m, ys[5] + 76f)
                switchBtn = Rc(m, ys[6], W - m, ys[6] + 52f)
                kbBottom = ys[5] + 76f
            }
            Screen.MAIN -> {
                val p = (bot - top) - (64f + 92f + 112f + 72f + 76f)
                val r = ((p - 150f) / 2f).coerceIn(80f, 140f)
                val ys = flow(top, bot, floatArrayOf(64f, 2f * r + 70f, 92f, 112f, 72f, 76f), 14f, 1000f)
                topBar = Rc(m, ys[0], W - m, ys[0] + 64f)
                avatarBtn = Rc(W - m - 56f, ys[0] + 4f, W - m, ys[0] + 60f)
                pr = r
                pcx = W / 2f
                pcy = ys[1] + 10f + r
                statusPill = Rc(W / 2f - 130f, pcy + r + 16f, W / 2f + 130f, pcy + r + 52f)
                val w = (W - 2f * m - 20f) / 3f
                stats = Array(3) { i -> Rc(m + i * (w + 10f), ys[2], m + i * (w + 10f) + w, ys[2] + 92f) }
                graph = Rc(m, ys[3], W - m, ys[3] + 112f)
                appsBtn = Rc(m, ys[4], W - m, ys[4] + 72f)
                val bw = (W - 2f * m - 12f) / 2f
                subBtn = Rc(m, ys[5], m + bw, ys[5] + 76f)
                outBtn = Rc(W - m - bw, ys[5], W - m, ys[5] + 76f)
                kbBottom = 0f
            }
            Screen.SUB -> {
                val ys = flow(top, bot, floatArrayOf(64f, 120f, 96f, 124f, 76f, 190f), 14f, 1000f)
                header = Rc(m, ys[0], W - m, ys[0] + 64f)
                back = Rc(m, ys[0] + 4f, m + 56f, ys[0] + 60f)
                statusCard = Rc(m, ys[1], W - m, ys[1] + 120f)
                trafficCard = Rc(m, ys[2], W - m, ys[2] + 96f)
                val w = (W - 2f * m - 20f) / 3f
                plans = Array(3) { i -> Rc(m + i * (w + 10f), ys[3], m + i * (w + 10f) + w, ys[3] + 124f) }
                buy = Rc(m, ys[4], W - m, ys[4] + 76f)
                keyLabel = Rc(m, ys[5], W - m, ys[5] + 14f)
                keyField = Rc(m, ys[5] + 20f, W - m, ys[5] + 84f)
                activate = Rc(m, ys[5] + 94f, W - m, ys[5] + 158f)
                keyMsg = Rc(m, ys[5] + 164f, W - m, ys[5] + 182f)
                kbBottom = ys[5] + 158f
            }
            Screen.APPS -> {
                header = Rc(m, top, W - m, top + 64f)
                back = Rc(m, top + 4f, m + 56f, top + 60f)
                appsMaster = Rc(m, top + 78f, W - m, top + 150f)
                appsSearch = Rc(m, top + 162f, W - m, top + 218f)
                appsApply = Rc(m, bot - 60f, W - m, bot)
                appsListTop = top + 230f
                appsListBottom = bot - 70f
                appsScrollBar = Rc(W - 12f, appsListTop, W - 6f, appsListBottom)
                kbBottom = top + 218f
            }
        }
    }
}

fun ap(v: Float, target: Float, rate: Float, dt: Float): Float = v + (target - v) * min(1f, dt * rate)
fun nowUnix(): Long = System.currentTimeMillis() / 1000L

class AuthOut(val token: String?, val me: JSONObject?, val err: String?)

class AppState(val ctx: Context) {
    val store = Store(ctx)
    val vpn = VpnController(ctx)
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    private var sampler: Job? = null
    private var token = store.token()
    private val deviceId = store.deviceId()
    private var refreshing = false
    private var keyBusy = false
    var requestVpn: ((Intent) -> Unit)? = null
    var pickAvatar: (() -> Unit)? = null

    var screen by mutableStateOf(Screen.LOGIN)
    var tick by mutableStateOf(0L)
    var s by mutableStateOf(1f)
    var ox by mutableStateOf(0f)
    var oy by mutableStateOf(0f)
    var inputUser by mutableStateOf("")
    var inputPass by mutableStateOf("")
    var inputKey by mutableStateOf("")
    var loginError by mutableStateOf("")

    var splitEnabled by mutableStateOf(false)
    var splitSel by mutableStateOf<Set<String>>(emptySet())
    var appliedSig by mutableStateOf("")
    var installed by mutableStateOf<List<AppInfo>>(emptyList())
    var appsLoading by mutableStateOf(false)
    var appsScroll by mutableStateOf(0f)
    var hitAppPkg: String? = null
    var reconnectHint by mutableStateOf(false)
    var searchQuery by mutableStateOf("")
    var searchFocus by mutableStateOf(false)
    var appsFiltered: List<AppInfo> = emptyList()

    var acc: Account? = null
    var avatar: ImageBitmap? = null
    var vstate by mutableStateOf(VState.OFF)
    var busy = false
    var authBusy = false
    var hit = Hit.NONE
    var down = Hit.NONE
    val hv = FloatArray(Hit.values().size)
    var time = 0f
    var scrTime = 0f
    var press = 0f
    var conn = 0f
    val planSel = floatArrayOf(0f, 1f, 0f)
    val focus = floatArrayOf(0f, 0f, 0f, 0f)
    var fUser = false
    var fPass = false
    var fKey = false
    var selPlan = 1
    var priceShown = PLANS[1].price.toFloat()
    var trafficShown = 0f
    var periodBar = 0f
    var shake = 0f
    var keyShake = 0f
    var rip = 0f
    var pingFade = 1f
    var ping = -1
    var failStreak = 0
    val pings = ArrayList<Int>()
    var downSpeed = 0f
    var upSpeed = 0f
    var connectedAt = 0L
    var errMsg: String? = null
    var errUntil = 0L
    var keyMsg = ""
    var keyMsgOk = false
    var keyMsgT = 0f
    var lastRefresh = 0L
    var lastSample = 0L
    var lastRx = 0L
    var lastTx = 0L
    var haveBase = false
    var screenW = 1f
    var screenH = 1f
    var areaW = W
    var areaH = 780f
    var shift = 0f
    var imeBottom = 0f
    var insetTop = 0f
    var insetBot = 0f
    var logoutArmedUntil = 0L
    val L = Layout()
    val particles = ArrayList<Particle>()

    init {
        val rnd = java.util.Random()
        for (i in 0 until 34) {
            particles.add(
                Particle(
                    rnd.nextFloat(), rnd.nextFloat(),
                    0.8f + rnd.nextFloat() * 1.6f,
                    6f + rnd.nextFloat() * 16f,
                    rnd.nextFloat() * 6.28f,
                    0.25f + rnd.nextFloat() * 0.75f
                )
            )
        }
        val sp = store.loadSplit()
        splitEnabled = sp.first
        splitSel = sp.second
        if (token.isNotEmpty()) restoreSession()
    }

    fun hvOf(h: Hit): Float = hv[h.ordinal]
    fun revealP(idx: Int): Float = ease((scrTime - idx * 0.07f) * 3.2f)
    fun shakeX(kind: Int): Float = when (kind) {
        1 -> sin(shake * 28f) * 8f * shake
        2 -> sin(keyShake * 28f) * 8f * keyShake
        else -> 0f
    }

    fun isPremium(): Boolean {
        val a = acc ?: return false
        return a.subExpire > nowUnix()
    }

    fun daysLeftSub(): Int {
        val a = acc ?: return 0
        val d = a.subExpire - nowUnix()
        return if (d <= 0) 0 else ((d + 86399) / 86400).toInt()
    }

    fun fmtDate(t: Long): String = SimpleDateFormat("dd.MM.yyyy", Locale.getDefault()).format(Date(t * 1000L))

    fun periodFrac(): Float {
        val a = acc ?: return 0f
        if (a.periodStart == 0L) return 0f
        return cl((nowUnix() - a.periodStart).toFloat() / PERIOD_SEC.toFloat())
    }

    fun setSize(w: Float, h: Float) {
        screenW = w
        screenH = h
        s = min(w / W, h / 720f)
        ox = (w - W * s) / 2f
        areaW = w / s
        areaH = h / s
    }

    fun isAppEnabled(pkg: String): Boolean = !splitEnabled || pkg in splitSel

    fun splitSummary(): String = if (!splitEnabled) "VPN для всех приложений" else "Только выбранные: " + splitSel.size

    fun filteredApps(): List<AppInfo> {
        if (searchQuery.isEmpty()) return installed
        val q = searchQuery.lowercase()
        return installed.filter { it.label.lowercase().contains(q) || it.pkg.lowercase().contains(q) }
    }

    fun maxScroll(): Float {
        val rows = ((L.appsListBottom - L.appsListTop) / APPS_ROW_H).toInt()
        val n = filteredApps().size
        val maxRows = (n - rows).coerceAtLeast(0)
        return maxRows * APPS_ROW_H
    }

    fun hitTest(p: Offset): Hit {
        val x = (p.x - ox) / s
        val y = (p.y - oy) / s
        when (screen) {
            Screen.LOGIN, Screen.REGISTER -> {
                if (L.loginBtn.has(x, y)) return Hit.LOGIN
                if (L.switchBtn.has(x, y)) return Hit.SWITCH
            }
            Screen.MAIN -> {
                val dx = x - L.pcx
                val dy = y - L.pcy
                if (dx * dx + dy * dy <= (L.pr + 6f) * (L.pr + 6f)) return Hit.CONNECT
                if (L.avatarBtn.has(x, y)) return Hit.AVATAR
                if (L.appsBtn.has(x, y)) return Hit.APPS
                if (L.subBtn.has(x, y)) return Hit.GO_SUB
                if (L.outBtn.has(x, y)) return Hit.DO_LOGOUT
            }
            Screen.SUB -> {
                if (L.back.has(x, y)) return Hit.BACK
                if (L.buy.has(x, y)) return Hit.BUY
                if (L.activate.has(x, y)) return Hit.ACTIVATE
                for (i in 0..2) if (L.plans[i].has(x, y)) return Hit.values()[Hit.PLAN0.ordinal + i]
            }
            Screen.APPS -> {
                if (L.back.has(x, y)) return Hit.BACK
                if (L.appsMaster.has(x, y)) return Hit.APPS_MASTER
                if (L.appsSearch.has(x, y)) return Hit.SEARCH
                if (splitEnabled) {
                    if (L.appsApply.has(x, y)) return Hit.APPLY_SPLIT
                    if (y in L.appsListTop..L.appsListBottom) {
                        val idx = ((y - L.appsListTop + appsScroll) / APPS_ROW_H).toInt()
                        val list = filteredApps()
                        if (idx in list.indices) {
                            hitAppPkg = list[idx].pkg
                            return Hit.APP_TOGGLE
                        }
                    }
                }
            }
        }
        return Hit.NONE
    }

    fun goTo(sc: Screen) {
        if (screen != sc) {
            screen = sc
            scrTime = 0f
            if (sc == Screen.SUB) {
                keyMsg = ""
                keyMsgT = 0f
            }
            if (sc == Screen.APPS) {
                appsScroll = 0f
                searchQuery = ""
                searchFocus = false
            }
        }
    }

    fun activate(h: Hit) {
        when (h) {
            Hit.GO_SUB -> goTo(Screen.SUB)
            Hit.APPS -> openApps()
            Hit.DO_LOGOUT -> {
                val now = System.currentTimeMillis()
                if (now < logoutArmedUntil) {
                    logoutArmedUntil = 0L
                    logout()
                } else {
                    logoutArmedUntil = now + 3000L
                }
            }
            Hit.CONNECT -> {
                rip = 0.001f
                toggle()
            }
            Hit.AVATAR -> pickAvatar?.invoke()
            Hit.LOGIN -> submitAuth()
            Hit.SWITCH -> {
                goTo(if (screen == Screen.LOGIN) Screen.REGISTER else Screen.LOGIN)
                loginError = ""
            }
            Hit.BUY -> openBuy()
            Hit.BACK -> { goTo(Screen.MAIN); appsScroll = 0f }
            Hit.PLAN0 -> selPlan = 0
            Hit.PLAN1 -> selPlan = 1
            Hit.PLAN2 -> selPlan = 2
            Hit.ACTIVATE -> redeemKey()
            Hit.APP_TOGGLE -> hitAppPkg?.let { toggleAppEnabled(it) }
            Hit.APPS_MASTER -> {
                splitEnabled = !splitEnabled
                if (!splitEnabled) appsScroll = 0f
                store.saveSplit(splitEnabled, splitSel)
                if (splitDirty()) reconnectHint = true
            }
            Hit.APPLY_SPLIT -> { reconnectHint = false; reconnectNow() }
            Hit.SEARCH -> searchFocus = true
            else -> {}
        }
    }

    fun toggleAppEnabled(pkg: String) {
        splitSel = if (pkg in splitSel) splitSel - pkg else splitSel + pkg
        store.saveSplit(splitEnabled, splitSel)
        if (splitDirty()) reconnectHint = true
    }

    fun update(dt: Float) {
        time += dt
        scrTime += dt
        val now = System.currentTimeMillis()
        L.compute(screen, areaH, insetTop / s, insetBot / s)
        for (h in Hit.values()) hv[h.ordinal] = ap(hv[h.ordinal], if (hit == h) 1f else 0f, 14f, dt)
        press = ap(press, if (down == Hit.CONNECT && hit == Hit.CONNECT) 1f else 0f, 22f, dt)
        conn = ap(conn, when (vstate) { VState.ON -> 1f; VState.CONNECTING -> 0.35f; else -> 0f }, 5f, dt)
        for (i in 0..2) planSel[i] = ap(planSel[i], if (selPlan == i) 1f else 0f, 12f, dt)
        val auth = screen == Screen.LOGIN || screen == Screen.REGISTER
        focus[0] = ap(focus[0], if (auth && fUser) 1f else 0f, 12f, dt)
        focus[1] = ap(focus[1], if (auth && fPass) 1f else 0f, 12f, dt)
        focus[2] = ap(focus[2], if (screen == Screen.SUB && fKey) 1f else 0f, 12f, dt)
        focus[3] = ap(focus[3], if (screen == Screen.APPS && searchFocus) 1f else 0f, 12f, dt)
        priceShown = ap(priceShown, PLANS[selPlan].price.toFloat(), 9f, dt)
        shake = max(0f, shake - dt * 2.2f)
        keyShake = max(0f, keyShake - dt * 2.2f)
        if (keyMsgT > 0f) keyMsgT = max(0f, keyMsgT - dt)
        pingFade = min(1f, pingFade + dt * 2.5f)
        if (rip > 0f) {
            rip += dt * 1.5f
            if (rip >= 1f) rip = 0f
        }
        val ms = maxScroll()
        if (appsScroll > ms) appsScroll = ms
        if (appsScroll < 0f) appsScroll = 0f

        val target = if (imeBottom > 0f && L.kbBottom > 0f) max(0f, (L.kbBottom + 14f) * s - (screenH - imeBottom)) else 0f
        shift = ap(shift, target, 12f, dt)
        val newOy = -shift
        if (kotlin.math.abs(newOy - oy) > 0.01f) oy = newOy

        val a = acc
        if (a != null) {
            trafficShown = ap(trafficShown, a.traffic.toFloat() / GB_F, 6f, dt)
            periodBar = ap(periodBar, if (isPremium()) periodFrac() else 0f, 4f, dt)
            if (vstate == VState.ON && !busy && !isPremium()) {
                showError("Подписка истекла")
                toggle()
            }
            if (now - lastRefresh >= 15000L) {
                lastRefresh = now
                refreshMe()
            }
        }
        if (vstate != VState.ON) {
            downSpeed *= 0.9f
            upSpeed *= 0.9f
        }

        val mul = 1f + conn * 1.6f
        val rnd = java.util.Random()
        for (p in particles) {
            p.y -= p.speed * mul * dt / areaH
            p.x += sin(time * 0.8f + p.phase) * 10f * dt / areaW
            if (p.y < -0.01f) {
                p.y = 1.01f
                p.x = rnd.nextFloat()
            }
        }
    }

    fun showError(msg: String) {
        errMsg = msg
        errUntil = System.currentTimeMillis() + 7000L
    }

    private fun setError(msg: String) {
        loginError = msg
        shake = 1f
    }

    fun loadAvatar(f: File) {
        avatar = if (f.exists()) BitmapFactory.decodeFile(f.absolutePath)?.asImageBitmap() else null
    }

    fun setAvatar(uri: Uri) {
        val a = acc ?: return
        scope.launch {
            val ok = withContext(Dispatchers.IO) {
                try {
                    val src = ctx.contentResolver.openInputStream(uri)?.use { BitmapFactory.decodeStream(it) }
                        ?: return@withContext false
                    val sz = min(src.width, src.height)
                    val crop = Bitmap.createBitmap(src, (src.width - sz) / 2, (src.height - sz) / 2, sz, sz)
                    val out = Bitmap.createScaledBitmap(crop, 160, 160, true)
                    store.avatarFile(a.user).outputStream().use { out.compress(Bitmap.CompressFormat.PNG, 100, it) }
                    true
                } catch (e: Exception) {
                    false
                }
            }
            if (ok) loadAvatar(store.avatarFile(a.user))
        }
    }

    private fun restoreSession() {
        scope.launch {
            val r = withContext(Dispatchers.IO) { Api.call("GET", "/api/me", token, null) }
            val j = r.json
            if (r.ok && j != null) {
                enterAccount(j)
            } else if (r.code == 401) {
                token = ""
                store.setToken("")
            } else {
                setError(r.error ?: "Нет связи с сервером")
            }
        }
    }

    private fun enterAccount(j: JSONObject) {
        val a = Account(j.optString("login"), j.optLong("sub_expire"), j.optLong("traffic"), j.optLong("period_start"))
        acc = a
        loadAvatar(store.avatarFile(a.user))
        inputUser = ""
        inputPass = ""
        loginError = ""
        trafficShown = 0f
        periodBar = 0f
        selPlan = 1
        priceShown = PLANS[1].price.toFloat()
        lastRefresh = System.currentTimeMillis()
        goTo(if (isPremium()) Screen.MAIN else Screen.SUB)
    }

    private fun applyMe(j: JSONObject) {
        val a = acc ?: return
        a.subExpire = j.optLong("sub_expire", a.subExpire)
        a.traffic = j.optLong("traffic", a.traffic)
        a.periodStart = j.optLong("period_start", a.periodStart)
    }

    fun refreshMe() {
        if (refreshing || token.isEmpty() || acc == null) return
        refreshing = true
        scope.launch {
            val r = withContext(Dispatchers.IO) { Api.call("GET", "/api/me", token, null) }
            refreshing = false
            val j = r.json
            if (r.ok && j != null) applyMe(j)
            else if (r.code == 401) forceLogout("Сессия истекла, войдите снова")
        }
    }

    private fun forceLogout(msg: String) {
        if (vstate == VState.ON) disconnect()
        token = ""
        store.setToken("")
        acc = null
        avatar = null
        goTo(Screen.LOGIN)
        setError(msg)
    }

    fun logout() {
        if (busy) return
        if (vstate == VState.ON) disconnect()
        val t = token
        token = ""
        store.setToken("")
        acc = null
        avatar = null
        goTo(Screen.LOGIN)
        scope.launch { withContext(Dispatchers.IO) { Api.call("POST", "/api/logout", t, null) } }
    }

    fun submitAuth() {
        val user = inputUser.trim()
        val pass = inputPass
        if (user.isEmpty() || pass.isEmpty() || authBusy) return
        val reg = screen == Screen.REGISTER
        if (reg && user.length < 3) {
            setError("Логин: минимум 3 символа")
            return
        }
        if (reg && pass.length < 6) {
            setError("Пароль: минимум 6 символов")
            return
        }
        authBusy = true
        scope.launch {
            val out = withContext(Dispatchers.IO) {
                val body = JSONObject().put("login", user).put("password", pass)
                val a = Api.call("POST", if (reg) "/api/register" else "/api/login", null, body)
                val j = a.json
                if (!a.ok || j == null) {
                    AuthOut(null, null, a.error ?: "Ошибка")
                } else {
                    val tok = j.optString("token")
                    val m = Api.call("GET", "/api/me", tok, null)
                    val mj = m.json
                    if (m.ok && mj != null) AuthOut(tok, mj, null) else AuthOut(null, null, m.error ?: "Ошибка")
                }
            }
            authBusy = false
            val tk = out.token
            val me = out.me
            if (tk != null && me != null) {
                token = tk
                store.setToken(tk)
                enterAccount(me)
            } else {
                setError(out.err ?: "Ошибка")
            }
        }
    }

    private fun keyResult(ok: Boolean, msg: String) {
        keyMsgOk = ok
        keyMsg = msg
        keyMsgT = 6f
        if (!ok) keyShake = 1f
    }

    fun redeemKey() {
        val a = acc ?: return
        if (inputKey.isEmpty() || keyBusy) return
        if (inputKey.length != 20) {
            keyResult(false, "Неверный формат ключа")
            return
        }
        keyBusy = true
        val k = inputKey
        scope.launch {
            val r = withContext(Dispatchers.IO) { Api.call("POST", "/api/redeem", token, JSONObject().put("key", k)) }
            keyBusy = false
            val j = r.json
            if (r.ok && j != null) {
                a.subExpire = j.optLong("sub_expire", a.subExpire)
                inputKey = ""
                keyResult(true, "Ключ активирован: +" + j.optInt("days") + " дн.")
                refreshMe()
            } else if (r.code == 401) {
                forceLogout("Сессия истекла, войдите снова")
            } else {
                keyResult(false, r.error ?: "Ошибка")
            }
        }
    }

    fun openBuy() {
        val i = Intent(Intent.ACTION_VIEW, Uri.parse(Cfg.BUY_URL + "?plan=" + PLANS[selPlan].code))
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        try {
            ctx.startActivity(i)
        } catch (e: Exception) {
        }
    }

    fun toggle() {
        if (busy || acc == null) return
        if (vstate == VState.OFF) {
            if (!isPremium()) {
                goTo(Screen.SUB)
                return
            }
            if (splitEnabled && splitSel.isEmpty()) {
                showError("Выберите приложения для VPN")
                return
            }
            val i = try {
                VpnService.prepare(ctx)
            } catch (e: Exception) {
                null
            }
            if (i != null) requestVpn?.invoke(i) else startConnect()
        } else {
            disconnect()
        }
    }

    fun onVpnPermission(granted: Boolean) {
        if (granted) startConnect() else showError("Нет разрешения VPN")
    }

    private fun startConnect() {
        if (busy) return
        busy = true
        errMsg = null
        vstate = VState.CONNECTING
        val useSplit = splitEnabled
        val spSel = splitSel
        appliedSig = sigOf(useSplit, spSel)
        scope.launch {
            val err = withContext(Dispatchers.IO) {
                var msg: String? = null
                val c = Api.call("POST", "/api/config", token, JSONObject().put("device", deviceId))
                val j = c.json
                if (!c.ok || j == null) {
                    msg = c.error ?: "Нет связи с сервером"
                } else {
                    try {
                        vpn.up(applySplit(j.optString("config"), useSplit, spSel))
                    } catch (ex: Exception) {
                        msg = "Туннель: " + (ex.message ?: ex.javaClass.simpleName).take(60)
                    }
                    if (msg == null) {
                        var ok = false
                        var i = 0
                        while (i < 6 && !ok) {
                            ok = vpn.ping() >= 0
                            if (!ok) delay(300)
                            i++
                        }
                        if (!ok) {
                            msg = "Сервер не отвечает"
                            try {
                                vpn.down()
                            } catch (ex: Exception) {
                            }
                        }
                    }
                }
                msg
            }
            onConnected(err)
        }
    }

    private fun onConnected(err: String?) {
        if (err == null) {
            vstate = VState.ON
            connectedAt = System.currentTimeMillis()
            lastSample = connectedAt
            haveBase = false
            downSpeed = 0f
            upSpeed = 0f
            failStreak = 0
            ping = -1
            pings.clear()
            startSampler()
            VpnServiceNotifier.start(ctx)
        } else {
            vstate = VState.OFF
            showError(err)
            refreshMe()
        }
        busy = false
    }

    private fun startSampler() {
        sampler?.cancel()
        sampler = scope.launch(Dispatchers.IO) {
            while (isActive) {
                val st = vpn.stats()
                val p = vpn.ping()
                withContext(Dispatchers.Main) { onSample(st, p) }
                delay(800)
            }
        }
    }

    private fun onSample(st: Pair<Long, Long>?, p: Int) {
        if (vstate != VState.ON) return
        val now = System.currentTimeMillis()
        val secs = (now - lastSample) / 1000f
        lastSample = now
        if (p >= 0) {
            failStreak = 0
            ping = p
            pingFade = 0f
            pings.add(p)
            if (pings.size > 40) pings.removeAt(0)
        } else {
            failStreak++
            ping = -1
        }
        if (st != null) {
            val rx = st.first
            val tx = st.second
            if (haveBase && rx >= lastRx && tx >= lastTx && secs > 0f) {
                downSpeed += ((rx - lastRx) / secs - downSpeed) * 0.35f
                upSpeed += ((tx - lastTx) / secs - upSpeed) * 0.35f
            }
            lastRx = rx
            lastTx = tx
            haveBase = true
        }
    }

    private fun sigOf(useSplit: Boolean, sel: Set<String>): String =
        if (!useSplit) "ALL" else "ONLY:" + sel.sorted().joinToString(",")

    fun splitDirty(): Boolean = vstate == VState.ON && sigOf(splitEnabled, splitSel) != appliedSig

    private fun applySplit(cfg: String, useSplit: Boolean, sel: Set<String>): String {
        if (!useSplit) return cfg
        if (sel.isEmpty()) return cfg
        val pkgs = sel + ctx.packageName
        val lines = cfg.lines().filterNot {
            val k = it.substringBefore("=").trim().lowercase()
            k == "includedapplications" || k == "excludedapplications"
        }.toMutableList()
        val idx = lines.indexOfFirst { it.trim().equals("[Interface]", ignoreCase = true) }
        if (idx < 0) return cfg
        lines.add(idx + 1, "IncludedApplications = " + pkgs.sorted().joinToString(", "))
        return lines.joinToString("\n")
    }

    fun openApps() {
        goTo(Screen.APPS)
        if (installed.isEmpty() && !appsLoading) loadInstalled()
    }

    private fun loadInstalled() {
        appsLoading = true
        scope.launch {
            val list = withContext(Dispatchers.IO) { queryApps() }
            installed = list
            appsLoading = false
        }
    }

    private fun queryApps(): List<AppInfo> {
        val pm = ctx.packageManager
        val launcher = Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
        val resolved = try {
            pm.queryIntentActivities(launcher, 0)
        } catch (e: Exception) {
            emptyList()
        }
        val pkgs = LinkedHashSet<String>()
        for (r in resolved) pkgs.add(r.activityInfo.packageName)
        try {
            for (app in pm.getInstalledApplications(PackageManager.GET_META_DATA)) {
                if (pm.getLaunchIntentForPackage(app.packageName) != null) pkgs.add(app.packageName)
            }
        } catch (e: Exception) {
        }
        val out = ArrayList<AppInfo>()
        for (pkg in pkgs) {
            if (pkg == ctx.packageName) continue
            val label = try {
                pm.getApplicationLabel(pm.getApplicationInfo(pkg, 0)).toString()
            } catch (e: Exception) {
                pkg
            }
            val icon = try {
                iconBitmap(pm.getApplicationIcon(pkg), 96)
            } catch (e: Exception) {
                null
            }
            out.add(AppInfo(label, pkg, icon))
        }
        out.sortBy { it.label.lowercase() }
        return out
    }

    private fun iconBitmap(d: Drawable, size: Int): ImageBitmap {
        val bmp = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        d.setBounds(0, 0, size, size)
        d.draw(Canvas(bmp))
        return bmp.asImageBitmap()
    }

    fun reconnectNow() {
        if (busy || vstate != VState.ON) return
        sampler?.cancel()
        sampler = null
        busy = true
        vstate = VState.CONNECTING
        scope.launch {
            withContext(Dispatchers.IO) {
                try {
                    vpn.down()
                } catch (e: Exception) {
                }
            }
            busy = false
            startConnect()
        }
    }

    private fun disconnect() {
        sampler?.cancel()
        sampler = null
        busy = true
        errMsg = null
        vstate = VState.OFF
        ping = -1
        pings.clear()
        VpnServiceNotifier.stop(ctx)
        scope.launch {
            withContext(Dispatchers.IO) {
                try {
                    vpn.down()
                } catch (e: Exception) {
                }
            }
            busy = false
        }
    }
}
