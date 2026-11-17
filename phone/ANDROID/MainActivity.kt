package com.nullvpn.app

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts

class MainActivity : ComponentActivity() {
    companion object {
        var shared: AppState? = null
    }

    private val vpnLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { r ->
            shared?.onVpnPermission(r.resultCode == RESULT_OK)
        }

    private val picker =
        registerForActivityResult(ActivityResultContracts.GetContent()) { uri ->
            if (uri != null) shared?.setAvatar(uri)
        }

    private val permLauncher =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.dark(Color.TRANSPARENT),
            navigationBarStyle = SystemBarStyle.dark(Color.TRANSPARENT)
        )

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                permLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
        }

        if (shared == null) shared = AppState(applicationContext)
        val st = shared!!
        st.requestVpn = { vpnLauncher.launch(it) }
        st.pickAvatar = { picker.launch("image/*") }
        setContent { NullVpnApp(st) }
    }
}