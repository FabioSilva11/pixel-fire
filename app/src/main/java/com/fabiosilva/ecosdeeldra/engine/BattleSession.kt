package com.fabiosilva.ecosdeeldra.engine

import android.content.Context
import android.os.SystemClock
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import com.fabiosilva.ecosdeeldra.R
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import okio.ByteString
import okio.ByteString.Companion.toByteString
import org.json.JSONObject
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.KeyPairGenerator
import java.security.KeyStore
import java.security.Signature
import java.security.cert.CertificateFactory
import java.security.spec.ECGenParameterSpec
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import javax.net.ssl.SSLContext
import javax.net.ssl.TrustManagerFactory
import javax.net.ssl.X509TrustManager

/** TLS protects every packet; P-256 proves device identity without embedding a server secret. */
class BattleSession(context: Context, private val listener: Listener) : AutoCloseable {
    interface Listener {
        fun onMessage(message: JSONObject)
        fun onSnapshot(state: FloatArray)
        fun onEffects(state: FloatArray)
        fun onStatus(text: String)
        fun onLatency(ms: Int)
    }
    companion object { const val HOST="187.102.244.208"; private const val ALIAS="zombie-zone-identity-v1" }
    private val worker=Executors.newSingleThreadScheduledExecutor()
    private val http: OkHttpClient
    @Volatile private var socket: WebSocket?=null
    @Volatile private var closed=false
    private var token=""
    private var tokenExpires=0L
    private var attempts=0
    private var disconnectedAt=0L
    private var generation=0
    private var lostGeneration=-1
    @Volatile private var desiredQueue: JSONObject?=null
    @Volatile private var inMatch=false
    @Volatile var leaving=false;private set
    @Volatile var connected=false; private set

    init {
        val trust=KeyStore.getInstance(KeyStore.getDefaultType()).apply { load(null) }
        context.resources.openRawResource(R.raw.server_ca).use {
            trust.setCertificateEntry("game",CertificateFactory.getInstance("X.509").generateCertificate(it))
        }
        val factory=TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm()).apply { init(trust) }
        val manager=factory.trustManagers.filterIsInstance<X509TrustManager>().single()
        val tls=SSLContext.getInstance("TLS").apply { init(null,arrayOf(manager),null) }
        http=OkHttpClient.Builder().sslSocketFactory(tls.socketFactory,manager)
            .connectTimeout(10,TimeUnit.SECONDS).readTimeout(15,TimeUnit.SECONDS).callTimeout(20,TimeUnit.SECONDS)
            .pingInterval(10,TimeUnit.SECONDS).build()
        worker.scheduleAtFixedRate({
            if(connected) {
                val ping=ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN).put(3).put(1).putShort(0)
                    .putDouble(SystemClock.elapsedRealtime().toDouble()).array()
                socket?.send(ping.toByteString())
            }
        },3,3,TimeUnit.SECONDS)
    }
    private fun api(path: String, data: JSONObject?=null, authenticated: Boolean=true): JSONObject {
        val request=Request.Builder().url("https://$HOST:8080$path")
        if(authenticated)request.header("Authorization","Bearer $token")
        if(data!=null)request.post(data.toString().toRequestBody("application/json".toMediaType()))
        return http.newCall(request.build()).execute().use { response ->
            val body=response.body?:error("Resposta vazia")
            require(body.contentLength()<=65536) { "Resposta muito grande" }
            val source=body.source();source.request(65537)
            require(source.buffer.size<=65536) { "Resposta muito grande" }
            val text=source.readUtf8()
            require(text.length<=65536) { "Resposta muito grande" }
            val result=JSONObject(text)
            if(!response.isSuccessful)error(result.optString("error","Serviço indisponível"))
            result
        }
    }
    private fun authenticate() {
        if(token.isNotEmpty()&&SystemClock.elapsedRealtime()<tokenExpires)return
        val keys=KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        if(!keys.containsAlias(ALIAS))KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC,"AndroidKeyStore").apply {
            initialize(KeyGenParameterSpec.Builder(ALIAS,KeyProperties.PURPOSE_SIGN).setAlgorithmParameterSpec(ECGenParameterSpec("secp256r1"))
                .setDigests(KeyProperties.DIGEST_SHA256).build());generateKeyPair()
        }
        val entry=keys.getEntry(ALIAS,null) as KeyStore.PrivateKeyEntry
        val challenge=api("/auth/challenge",JSONObject().put("publicKey",Base64.encodeToString(entry.certificate.publicKey.encoded,Base64.NO_WRAP)),false)
        val signature=Signature.getInstance("SHA256withECDSA").apply { initSign(entry.privateKey);update(Base64.decode(challenge.getString("nonce"),Base64.DEFAULT)) }.sign()
        val result=api("/auth/complete",JSONObject().put("id",challenge.getString("id")).put("signature",Base64.encodeToString(signature,Base64.NO_WRAP)),false)
        token=result.getString("token");tokenExpires=SystemClock.elapsedRealtime()+result.getLong("expiresIn")*1000-60000
    }
    fun connect() { worker.execute { open() } }
    @Synchronized private fun open() {
        if(closed||connected||socket!=null)return
        val id=++generation
        try {
            listener.onStatus(if(inMatch)"Reconectando…" else "Conectando ao servidor…")
            authenticate()
            socket=http.newWebSocket(Request.Builder().url("wss://$HOST:27015/play").header("Authorization","Bearer $token").build(),object: WebSocketListener() {
                override fun onOpen(webSocket: WebSocket,response: Response) {
                    if(id!=generation||closed){webSocket.close(1000,"Encerrado");return}
                    connected=true
                }
                override fun onMessage(webSocket: WebSocket,text: String) {
                    if(id!=generation||closed)return
                    try {
                        require(text.length<=65536);val m=JSONObject(text)
                        when(m.optString("type")) {
                            "connected" -> { inMatch=false;recovered();if(leaving)webSocket.send("{\"type\":\"leave\"}")else desiredQueue?.let { webSocket.send(it.toString()) } }
                            "queued" -> desiredQueue=null
                            "start" -> { if(leaving){webSocket.send("{\"type\":\"leave\"}");return};inMatch=true;desiredQueue=null }
                            "cancelled","matchClosed" -> { inMatch=false;desiredQueue=null;leaving=false }
                        }
                        listener.onMessage(m)
                    } catch(_: Exception) { webSocket.close(1002,"Resposta inválida") }
                }
                override fun onMessage(webSocket: WebSocket,bytes: ByteString) {
                    if(id!=generation||closed)return
                    if(bytes.size==12&&bytes[0]==4.toByte()&&bytes[1]==1.toByte()&&bytes[2]==0.toByte()&&bytes[3]==0.toByte()) {
                        val sent=ByteBuffer.wrap(bytes.toByteArray()).order(ByteOrder.LITTLE_ENDIAN).getDouble(4)
                        if(sent.isFinite())listener.onLatency((SystemClock.elapsedRealtime()-sent).toInt().coerceIn(0,60000))
                    } else if(bytes.size>0&&bytes[0]==5.toByte()) {
                        BattleWire.effects(bytes.toByteArray())?.let{listener.onEffects(it)}?:webSocket.close(1002,"Efeito inválido")
                    } else BattleWire.snapshot(bytes.toByteArray())?.let { recovered();listener.onSnapshot(it) }
                        ?:webSocket.close(1002,"Estado inválido")
                }
                override fun onClosing(webSocket: WebSocket,code: Int,reason: String) { webSocket.close(if(code==1005)1000 else code,null) }
                override fun onClosed(webSocket: WebSocket,code: Int,reason: String) { android.util.Log.i("ZZBattle","socket_closed code=$code");lost(id,code) }
                override fun onFailure(webSocket: WebSocket,t: Throwable,response: Response?) { lost(id,response?.code?:0) }
            })
        } catch(_: Exception) { lost(id,0) }
    }
    @Synchronized private fun lost(id: Int,code: Int) {
        if(id!=generation||closed||lostGeneration==id)return
        lostGeneration=id
        socket=null;connected=false
        if(code==4001||code==401){token="";tokenExpires=0}
        if(disconnectedAt==0L)disconnectedAt=SystemClock.elapsedRealtime()
        if(++attempts>6||(inMatch&&SystemClock.elapsedRealtime()-disconnectedAt>=28000)) {
            listener.onStatus("Conexão interrompida. Volte ao lobby para tentar novamente.");return
        }
        listener.onStatus("Conexão perdida. Reconectando…")
        worker.schedule({ open() },(500L shl (attempts-1).coerceAtMost(3)),TimeUnit.MILLISECONDS)
    }
    @Synchronized private fun recovered(){attempts=0;disconnectedAt=0}
    fun queue(teamSize: Int,autoFill: Boolean,mapHash: Long,look: Int,training: Boolean=false) {
        require(teamSize in listOf(1,2,4))
        val m=JSONObject().put("type","queue").put("teamSize",teamSize).put("autoFill",autoFill).put("mapHash",mapHash).put("protocol",1).put("look",look).put("training",training)
        desiredQueue=m
        if(connected)socket?.send(m.toString())else connect()
    }
    fun ready(mapHash: Long,look: Int) { socket?.send(JSONObject().put("type","ready").put("mapHash",mapHash).put("protocol",1).put("look",look).toString()) }
    fun input(command: FloatArray) {
        val ws=socket?:return
        if(!connected||!inMatch)return
        if(ws.queueSize()>32768){ws.cancel();return}
        BattleWire.input(command)?.let { ws.send(it.toByteString()) }
    }
    fun cancelQueue() { desiredQueue=null;socket?.send("{\"type\":\"cancel\"}") }
    fun leaveMatch() { desiredQueue=null;inMatch=false;leaving=true;socket?.send("{\"type\":\"leave\"}");if(!connected)connect() }
    fun party(action: String,teamSize: Int=1,autoFill: Boolean=true,code: String="") {
        require(action in listOf("create","join","leave","get"))
        worker.execute {
            try {
                authenticate()
                val p=api(if(action=="get")"/party" else "/party/$action",if(action=="get")null else JSONObject().put("teamSize",teamSize).put("autoFill",autoFill).put("code",code))
                listener.onMessage(p.put("type","party"))
            } catch(e: Exception){listener.onStatus(e.message?:"Não foi possível alterar o grupo")}
        }
    }
    override fun close() {
        closed=true;++generation;connected=false;socket?.close(1000,"Saindo");socket=null
        worker.shutdownNow();http.dispatcher.executorService.shutdown();http.connectionPool.evictAll();token=""
    }
}
