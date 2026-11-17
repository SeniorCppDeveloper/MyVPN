package com.nullvpn.app

import android.content.Context
import java.io.File
import java.util.UUID

data class Account(
    var user: String,
    var subExpire: Long,
    var traffic: Long,
    var periodStart: Long
)

class Store(private val ctx: Context) {
    private fun f(name: String) = File(ctx.filesDir, name)

    fun token(): String = if (f("token.dat").exists()) f("token.dat").readText().trim() else ""

    fun setToken(t: String) {
        if (t.isEmpty()) f("token.dat").delete() else f("token.dat").writeText(t)
    }

    fun deviceId(): String {
        val file = f("device.dat")
        if (file.exists()) {
            val s = file.readText().trim()
            if (s.isNotEmpty()) return s
        }
        val id = "android-" + UUID.randomUUID().toString().replace("-", "").take(12)
        file.writeText(id)
        return id
    }

    fun avatarFile(user: String): File =
        File(ctx.filesDir, "avatar_" + user.lowercase().toByteArray().joinToString("") { "%02x".format(it) } + ".png")
}