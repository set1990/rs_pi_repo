package com.example.intercom_mobile_app

import android.os.Handler
import android.os.Looper
import android.os.Process
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "intercom/native")
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    "closeApp" -> {
                        result.success(null)
                        finishAndRemoveTask()
                    }
                    "killApp" -> {
                        result.success(null)
                        finishAndRemoveTask()
                        Handler(Looper.getMainLooper()).postDelayed({
                            Process.killProcess(Process.myPid())
                        }, 300)
                    }
                    else -> result.notImplemented()
                }
            }
    }
}
