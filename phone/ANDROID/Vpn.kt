package com.nullvpn.app

import android.content.Context
import com.wireguard.android.backend.GoBackend
import com.wireguard.android.backend.Tunnel
import com.wireguard.config.Config
import java.io.ByteArrayInputStream
import java.net.InetSocketAddress
import java.net.Socket

class VpnController(ctx: Context) {
    private val backend = GoBackend(ctx)

    private val tunnel = object : Tunnel {
        override fun getName(): String = "nullvpn"
        override fun onStateChange(newState: Tunnel.State) {}
    }

    fun up(configText: String) {
        val cfg = Config.parse(ByteArrayInputStream(configText.toByteArray()))
        backend.setState(tunnel, Tunnel.State.UP, cfg)
    }

    fun down() {
        backend.setState(tunnel, Tunnel.State.DOWN, null)
    }

    fun stats(): Pair<Long, Long>? {
        return try {
            val s = backend.getStatistics(tunnel)
            Pair(s.totalRx(), s.totalTx())
        } catch (e: Exception) {
            null
        }
    }

    fun ping(): Int {
        val t0 = System.nanoTime()
        return try {
            Socket().use { it.connect(InetSocketAddress("1.1.1.1", 443), 1500) }
            val ms = ((System.nanoTime() - t0) / 1_000_000L).toInt()
            if (ms < 1) 1 else ms
        } catch (e: Exception) {
            -1
        }
    }
}