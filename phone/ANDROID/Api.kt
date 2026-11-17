package com.nullvpn.app

import org.json.JSONObject
import java.net.URL
import java.security.MessageDigest
import java.security.cert.CertificateException
import java.security.cert.X509Certificate
import javax.net.ssl.HttpsURLConnection
import javax.net.ssl.SSLContext
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager

class ApiResult(val ok: Boolean, val code: Int, val json: JSONObject?, val error: String?)

object Api {
    private val pinned = Cfg.CERT_SHA256.replace(":", "").lowercase()

    private val trust = object : X509TrustManager {
        override fun checkClientTrusted(chain: Array<X509Certificate>, authType: String) {}
        override fun checkServerTrusted(chain: Array<X509Certificate>, authType: String) {
            if (chain.isEmpty()) throw CertificateException("empty chain")
            val d = MessageDigest.getInstance("SHA-256").digest(chain[0].encoded)
            val hex = d.joinToString("") { "%02x".format(it) }
            if (hex != pinned) throw CertificateException("pin mismatch")
        }
        override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
    }

    private val ssl: SSLContext by lazy {
        val c = SSLContext.getInstance("TLS")
        c.init(null, arrayOf<TrustManager>(trust), null)
        c
    }

    fun call(method: String, path: String, token: String?, body: JSONObject?): ApiResult {
        return try {
            val c = URL(Cfg.API_URL + path).openConnection() as HttpsURLConnection
            c.sslSocketFactory = ssl.socketFactory
            c.hostnameVerifier = javax.net.ssl.HostnameVerifier { _, _ -> true }
            c.requestMethod = method
            c.connectTimeout = 8000
            c.readTimeout = 15000
            c.setRequestProperty("Content-Type", "application/json")
            if (token != null && token.isNotEmpty()) c.setRequestProperty("Authorization", "Bearer $token")
            if (body != null) {
                c.doOutput = true
                c.outputStream.use { it.write(body.toString().toByteArray()) }
            }
            val code = c.responseCode
            val stream = if (code in 200..299) c.inputStream else c.errorStream
            val text = stream?.bufferedReader()?.use { it.readText() } ?: ""
            val j = try {
                JSONObject(text)
            } catch (e: Exception) {
                null
            }
            if (code in 200..299) ApiResult(true, code, j, null)
            else ApiResult(false, code, j, j?.optString("detail")?.ifEmpty { null } ?: "Ошибка сервера ($code)")
        } catch (e: Exception) {
            ApiResult(false, 0, null, "Нет связи с сервером")
        }
    }
}