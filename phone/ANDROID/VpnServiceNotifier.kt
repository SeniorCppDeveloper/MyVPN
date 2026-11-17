package com.nullvpn.app

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat

class VpnServiceNotifier : Service() {
    private var lastUpdate = 0L
    private var lastVState: VState = VState.OFF

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == "STOP") {
            MainActivity.shared?.toggle()
            stopSelf()
            return START_NOT_STICKY
        }

        createChannel()
        startForeground(NOTIF_ID, buildNotification("00:00:00", "—"))

        Thread {
            while (!Thread.currentThread().isInterrupted) {
                val a = MainActivity.shared
                if (a != null) {
                    val now = System.currentTimeMillis()
                    if (a.vstate != lastVState) {
                        lastVState = a.vstate
                        lastUpdate = 0L
                    }

                    if (a.vstate == VState.ON) {
                        if (now - lastUpdate > 1000) {
                            lastUpdate = now
                            val ping = if (a.ping > 0) "${a.ping} мс" else "—"
                            val speed = String.format("%.1f МБ/с", a.downSpeed / 1048576f)
                            val secs = (now - a.connectedAt) / 1000L
                            updateNotification(secs, ping, speed, VState.ON)
                        }
                    } else if (a.vstate == VState.CONNECTING) {
                        if (now - lastUpdate > 1000) {
                            lastUpdate = now
                            updateNotification(0L, "—", "—", VState.CONNECTING)
                        }
                    }
                }
                Thread.sleep(500)
            }
        }.start()

        return START_STICKY
    }

    private fun createChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val ch = NotificationChannel(
                CHANNEL_ID,
                "VPN Статус",
                NotificationManager.IMPORTANCE_LOW
            )
            ch.description = "Показывает статус VPN-подключения"
            ch.setShowBadge(false)
            ch.setSound(null, null)
            val nm = getSystemService(NotificationManager::class.java)
            nm.createNotificationChannel(ch)
        }
    }

    private fun buildNotification(time: String, ping: String): Notification {
        val intent = Intent(this, MainActivity::class.java)
        val pi = PendingIntent.getActivity(this, 0, intent, PendingIntent.FLAG_IMMUTABLE)

        val stopIntent = Intent(this, VpnServiceNotifier::class.java).setAction("STOP")
        val stopPi = PendingIntent.getService(this, 1, stopIntent, PendingIntent.FLAG_IMMUTABLE)

        val text = "Время: $time • Пинг: $ping"

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.ic_lock_lock)
            .setContentTitle("VPN Защищено")
            .setContentText(text)
            .setStyle(NotificationCompat.BigTextStyle().bigText(text))
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setContentIntent(pi)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .addAction(android.R.drawable.ic_media_pause, "Остановить", stopPi)
            .build()
    }

    private fun updateNotification(secs: Long, ping: String, speed: String, state: VState) {
        val nm = getSystemService(NotificationManager::class.java)
        val timeStr = String.format("%02d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60)
        val text = if (state == VState.ON) {
            "Время: $timeStr • Пинг: $ping • $speed"
        } else {
            "Подключение..."
        }

        val intent = Intent(this, MainActivity::class.java)
        val pi = PendingIntent.getActivity(this, 0, intent, PendingIntent.FLAG_IMMUTABLE)
        val stopIntent = Intent(this, VpnServiceNotifier::class.java).setAction("STOP")
        val stopPi = PendingIntent.getService(this, 1, stopIntent, PendingIntent.FLAG_IMMUTABLE)

        val notif = NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.ic_lock_lock)
            .setContentTitle(if (state == VState.ON) "VPN Защищено" else "VPN Подключение")
            .setContentText(text)
            .setStyle(NotificationCompat.BigTextStyle().bigText(text))
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setContentIntent(pi)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .addAction(android.R.drawable.ic_media_pause, "Остановить", stopPi)
            .build()

        nm.notify(NOTIF_ID, notif)
    }

    override fun onDestroy() {
        super.onDestroy()
        val nm = getSystemService(NotificationManager::class.java)
        nm.cancel(NOTIF_ID)
    }

    companion object {
        const val CHANNEL_ID = "vpn_status_channel"
        const val NOTIF_ID = 1001

        fun start(ctx: Context) {
            val i = Intent(ctx, VpnServiceNotifier::class.java)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                ctx.startForegroundService(i)
            } else {
                ctx.startService(i)
            }
        }

        fun stop(ctx: Context) {
            ctx.stopService(Intent(ctx, VpnServiceNotifier::class.java))
        }
    }
}