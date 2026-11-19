package com.nullvpn.app

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.detectVerticalDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.ime
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.layout
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardCapitalization
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.OffsetMapping
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.TransformedText
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.Constraints
import androidx.compose.ui.unit.IntOffset
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

class KeyVT : VisualTransformation {
    override fun filter(text: AnnotatedString): TransformedText {
        val raw = text.text
        val sb = StringBuilder()
        for (i in raw.indices) {
            if (i > 0 && i % 5 == 0) sb.append('-')
            sb.append(raw[i])
        }
        val map = object : OffsetMapping {
            override fun originalToTransformed(offset: Int): Int = if (offset <= 0) 0 else offset + (offset - 1) / 5
            override fun transformedToOriginal(offset: Int): Int = min(raw.length, offset - offset / 6)
        }
        return TransformedText(AnnotatedString(sb.toString()), map)
    }
}

fun filterKey(s: String): String {
    val sb = StringBuilder()
    for (c in s.uppercase()) {
        if ((c in 'A'..'Z' || c in '2'..'7') && sb.length < 20) sb.append(c)
    }
    return sb.toString()
}

@Composable
fun Overlay(a: AppState, rc: () -> Rc, idx: Int, kind: Int, content: @Composable () -> Unit) {
    Box(
        modifier = Modifier
            .offset {
                val tk = a.tick
                val r = rc()
                IntOffset(
                    (a.ox + (r.l + a.shakeX(kind)) * a.s).roundToInt(),
                    (a.oy + r.t * a.s).roundToInt()
                )
            }
            .layout { m, _ ->
                val tk = a.tick
                val r = rc()
                val w = (r.w * a.s).roundToInt().coerceAtLeast(1)
                val h = (r.h * a.s).roundToInt().coerceAtLeast(1)
                val p = m.measure(Constraints.fixed(w, h))
                layout(w, h) { p.place(0, 0) }
            }
            .graphicsLayer {
                val tk = a.tick
                alpha = a.revealP(idx)
            },
        contentAlignment = Alignment.CenterStart
    ) { content() }
}

@Composable
fun InputField(
    a: AppState,
    value: String,
    onChange: (String) -> Unit,
    keyboard: KeyboardOptions,
    actions: KeyboardActions,
    vt: VisualTransformation,
    onFocus: (Boolean) -> Unit
) {
    val d = LocalDensity.current
    BasicTextField(
        value = value,
        onValueChange = onChange,
        singleLine = true,
        textStyle = TextStyle(color = Color.White, fontSize = with(d) { (16f * a.s).toSp() }),
        cursorBrush = SolidColor(Color(0xFFDCB4FF)),
        visualTransformation = vt,
        keyboardOptions = keyboard,
        keyboardActions = actions,
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = with(d) { (50f * a.s).toDp() }, end = with(d) { (12f * a.s).toDp() })
            .onFocusChanged { onFocus(it.isFocused) }
    )
}

@Composable
fun NullVpnApp(a: AppState) {
    val focusManager = LocalFocusManager.current
    val d = LocalDensity.current
    a.imeBottom = WindowInsets.ime.getBottom(d).toFloat()
    a.insetTop = WindowInsets.statusBars.getTop(d).toFloat()
    a.insetBot = WindowInsets.navigationBars.getBottom(d).toFloat()

    LaunchedEffect(Unit) {
        var last = withFrameNanos { it }
        while (true) {
            val n = withFrameNanos { it }
            var dt = (n - last) / 1_000_000_000f
            last = n
            if (dt > 0.05f) dt = 0.05f
            a.update(dt)
            a.tick = n
        }
    }

    BackHandler(enabled = a.screen == Screen.SUB || a.screen == Screen.APPS) {
        a.goTo(Screen.MAIN)
        a.appsScroll = 0f
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(2, 1, 6))
            .onSizeChanged { a.setSize(it.width.toFloat(), it.height.toFloat()) }
            .pointerInput(Unit) {
                detectVerticalDragGestures(
                    onVerticalDrag = { change, dragAmount ->
                        if (a.screen == Screen.APPS && a.splitEnabled) {
                            change.consume()
                            a.appsScroll = (a.appsScroll - dragAmount / a.s)
                                .coerceIn(0f, max(0f, a.maxScroll()))
                        }
                    }
                )
            }
            .pointerInput(Unit) {
                detectTapGestures(onPress = { pos ->
                    val px = (pos.x - a.ox) / a.s
                    val py = (pos.y - a.oy) / a.s
                    if (a.screen == Screen.APPS && a.splitEnabled && a.L.appsScrollBar.has(px, py)) {
                        val list = a.filteredApps()
                        val maxRows = ((a.L.appsListBottom - a.L.appsListTop) / APPS_ROW_H).toInt()
                        val total = list.size * APPS_ROW_H
                        val view = a.L.appsListBottom - a.L.appsListTop
                        val thumbH = max(30f, view * view / total)
                        val barH = a.L.appsScrollBar.h
                        val maxScroll = (total - view).coerceAtLeast(1f)
                        val rel = (py - a.L.appsListTop - thumbH / 2f).coerceIn(0f, barH - thumbH) / (barH - thumbH)
                        a.appsScroll = rel * maxScroll
                        change@ while (true) {
                            val ok = tryAwaitRelease()
                            if (!ok) break
                            return@detectTapGestures
                        }
                    }
                    focusManager.clearFocus()
                    val h = a.hitTest(pos)
                    a.hit = h
                    a.down = h
                    val ok = tryAwaitRelease()
                    a.hit = Hit.NONE
                    a.down = Hit.NONE
                    if (ok && h != Hit.NONE) a.activate(h)
                })
            }
    ) {
        Canvas(Modifier.fillMaxSize()) {
            val tk = a.tick
            paintAll(a)
        }

        if (a.screen == Screen.LOGIN || a.screen == Screen.REGISTER) {
            Overlay(a, { a.L.fieldUser }, 2, 1) {
                InputField(
                    a, a.inputUser,
                    { a.inputUser = it.replace("\t", "").replace("\n", "").take(20); a.loginError = "" },
                    KeyboardOptions(imeAction = ImeAction.Next),
                    KeyboardActions(),
                    VisualTransformation.None
                ) { a.fUser = it }
            }
            Overlay(a, { a.L.fieldPass }, 3, 1) {
                InputField(
                    a, a.inputPass,
                    { a.inputPass = it.replace("\n", "").take(32); a.loginError = "" },
                    KeyboardOptions(keyboardType = KeyboardType.Password, imeAction = ImeAction.Done),
                    KeyboardActions(onDone = { a.submitAuth() }),
                    PasswordVisualTransformation()
                ) { a.fPass = it }
            }
        }

        if (a.screen == Screen.SUB) {
            Overlay(a, { a.L.keyField }, 7, 2) {
                InputField(
                    a, a.inputKey,
                    { a.inputKey = filterKey(it) },
                    KeyboardOptions(
                        capitalization = KeyboardCapitalization.Characters,
                        keyboardType = KeyboardType.Ascii,
                        imeAction = ImeAction.Done
                    ),
                    KeyboardActions(onDone = { a.redeemKey() }),
                    KeyVT()
                ) { a.fKey = it }
            }
        }

        if (a.screen == Screen.APPS && a.splitEnabled) {
            Overlay(a, { a.L.appsSearch }, 2, 3) {
                InputField(
                    a, a.searchQuery,
                    { a.searchQuery = it; a.appsScroll = 0f },
                    KeyboardOptions(imeAction = ImeAction.Search),
                    KeyboardActions(onSearch = { focusManager.clearFocus() }),
                    VisualTransformation.None
                ) { a.searchFocus = it }
            }
        }
    }
}
