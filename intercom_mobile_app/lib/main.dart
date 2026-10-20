import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_foreground_task/flutter_foreground_task.dart';

import 'udp_task_handler.dart';

const MethodChannel _native = MethodChannel('intercom/native');

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  FlutterForegroundTask.initCommunicationPort();
  runApp(const IntercomApp());
}

class IntercomApp extends StatelessWidget {
  const IntercomApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Intercom Mobile',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.blue),
        useMaterial3: true,
      ),
      home: const IntercomScreen(),
      debugShowCheckedModeBanner: false,
    );
  }
}

enum AppState { waiting, ringing, connected }

class IntercomScreen extends StatefulWidget {
  const IntercomScreen({super.key});

  @override
  State<IntercomScreen> createState() => _IntercomScreenState();
}

class _IntercomScreenState extends State<IntercomScreen>
    with WidgetsBindingObserver {
  AppState _currentState = AppState.waiting;
  String _lastSenderIp = '';

  bool _foreground = true;
  Timer? _stateTimer;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    FlutterForegroundTask.addTaskDataCallback(_onServiceData);

    _requestState();
    _stateTimer = Timer.periodic(const Duration(milliseconds: 700), (_) {
      if (_foreground && _currentState == AppState.waiting) {
        _requestState();
      }
    });

    WidgetsBinding.instance.addPostFrameCallback((_) => _startService());
  }

  @override
  void dispose() {
    _stateTimer?.cancel();
    WidgetsBinding.instance.removeObserver(this);
    FlutterForegroundTask.removeTaskDataCallback(_onServiceData);
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    _foreground = state == AppLifecycleState.resumed;
    if (_foreground) _requestState();
  }


  Future<void> _startService() async {
    if (!Platform.isAndroid) return;

    final permission = await FlutterForegroundTask.checkNotificationPermission();
    if (permission != NotificationPermission.granted) {
      await FlutterForegroundTask.requestNotificationPermission();
    }
    if (!await FlutterForegroundTask.isIgnoringBatteryOptimizations) {
      await FlutterForegroundTask.requestIgnoreBatteryOptimization();
    }
    if (!await FlutterForegroundTask.canDrawOverlays) {
      await FlutterForegroundTask.openSystemAlertWindowSettings();
    }

    FlutterForegroundTask.init(
      androidNotificationOptions: AndroidNotificationOptions(
        channelId: 'intercom_service_v2',
        channelName: 'Intercom in background',
        channelDescription: 'Intercom',
        channelImportance: NotificationChannelImportance.MIN,
        priority: NotificationPriority.MIN,
        visibility: NotificationVisibility.VISIBILITY_SECRET,
        onlyAlertOnce: true,
        showWhen: false,
        showBadge: false,
      ),
      iosNotificationOptions: const IOSNotificationOptions(),
      foregroundTaskOptions: ForegroundTaskOptions(
        eventAction: ForegroundTaskEventAction.nothing(),
        autoRunOnBoot: true,
        allowWakeLock: true,
        allowWifiLock: true,
      ),
    );

    if (!await FlutterForegroundTask.isRunningService) {
      await FlutterForegroundTask.startService(
        serviceId: 256,
        notificationTitle: 'Intercom',
        notificationText: 'Intercom active',
        callback: startCallback,
      );
    }

    _requestState();
  }

  void _sendToService(Map<String, Object> data) {
    try {
      FlutterForegroundTask.sendDataToTask(data);
    } catch (_) {}
  }

  void _requestState() => _sendToService({'cmd': 'getState'});

  void _onServiceData(Object data) {
    if (data is! Map) return;

    final event = data['event'];
    final ringing =
        event == 'ring' || (event == 'state' && data['ringing'] == true);

    if (ringing && _currentState == AppState.waiting && mounted) {
      setState(() {
        _currentState = AppState.ringing;
        _lastSenderIp = (data['ip'] as String?) ?? '';
      });
    }
  }


  void _sendUdpPacket(String message) {
    _sendToService({
      'cmd': 'send',
      'message': message,
      'ip': _lastSenderIp,
    });
  }


  void _onAcceptPressed() {
    _sendUdpPacket('CMD_ACCEPT');
    _sendToService({'cmd': 'accepted'});
    setState(() => _currentState = AppState.connected);
  }

  void _onRejectPressed() {
    _sendUdpPacket('CMD_REJECT');
    _sendToService({'cmd': 'finish'});
    setState(() => _currentState = AppState.waiting);
    _closeWindow();
  }

  void _onOpenPressed() {
    _sendUdpPacket('CMD_OPEN');
  }

  void _onEndPressed() {
    _sendUdpPacket('CMD_END');
    _sendToService({'cmd': 'finish'});
    setState(() => _currentState = AppState.waiting);
    _closeWindow();
  }


  Future<void> _closeWindow() async {
    await Future.delayed(const Duration(milliseconds: 400));
    try {
      await _native.invokeMethod('closeApp');
    } catch (_) {
      await SystemNavigator.pop();
    }
  }

  void _onRunPressed() => _closeWindow();

  Future<void> _onExitPressed() async {
    try {
      await FlutterForegroundTask.stopService();
    } catch (_) {}
    try {
      await _native.invokeMethod('killApp');
    } catch (_) {
      await SystemNavigator.pop();
    }
  }


  @override
  Widget build(BuildContext context) {
    return WithForegroundTask(
      child: Scaffold(
        appBar: AppBar(
          title: Text('Intercom Mobile (${_currentState.name.toUpperCase()})'),
          backgroundColor: Theme.of(context).colorScheme.inversePrimary,
        ),
        body: Center(
          child: Padding(
            padding: const EdgeInsets.all(24.0),
            child: _buildBodyContent(),
          ),
        ),
      ),
    );
  }

  Widget _wideButton({
    required VoidCallback onPressed,
    required IconData icon,
    required String label,
    required Color color,
  }) {
    return SizedBox(
      width: 240,
      child: ElevatedButton.icon(
        onPressed: onPressed,
        icon: Icon(icon),
        label: Text(label),
        style: ElevatedButton.styleFrom(
          backgroundColor: color,
          foregroundColor: Colors.white,
        ),
      ),
    );
  }

  Widget _buildBodyContent() {
    switch (_currentState) {
      case AppState.waiting:
        return Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Image(
              image: AssetImage('assets/images/logo.png'),
              width: 300,
              height: 300,
            ),
            const SizedBox(height: 20),
            const Text(
              'Waiting For The Call...',
              style: TextStyle(fontSize: 18),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 40),
            _wideButton(
              onPressed: _onRunPressed,
              icon: Icons.play_arrow,
              label: 'RUN',
              color: Colors.green,
            ),
            const SizedBox(height: 12),
            _wideButton(
              onPressed: _onExitPressed,
              icon: Icons.power_settings_new,
              label: 'EXIT',
              color: Colors.red,
            ),
          ],
        );

      case AppState.ringing:
        return Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.ring_volume, size: 100, color: Colors.orange),
            const SizedBox(height: 20),
            const Text(
              'Call!!!',
              style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
            ),
            Text('from: $_lastSenderIp',
                style: const TextStyle(color: Colors.grey)),
            const SizedBox(height: 40),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: [
                ElevatedButton.icon(
                  onPressed: _onRejectPressed,
                  icon: const Icon(Icons.call_end),
                  label: const Text('Decline'),
                  style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.red,
                      foregroundColor: Colors.white),
                ),
                ElevatedButton.icon(
                  onPressed: _onAcceptPressed,
                  icon: const Icon(Icons.call),
                  label: const Text('Accept'),
                  style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.green,
                      foregroundColor: Colors.white),
                ),
              ],
            ),
          ],
        );

      case AppState.connected:
        return Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.phone_in_talk, size: 100, color: Colors.green),
            const SizedBox(height: 20),
            const Text(
              'Active call',
              style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 40),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: [
                ElevatedButton.icon(
                  onPressed: _onOpenPressed,
                  icon: const Icon(Icons.lock_open),
                  label: const Text('Open door'),
                  style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.blue,
                      foregroundColor: Colors.white),
                ),
                ElevatedButton.icon(
                  onPressed: _onEndPressed,
                  icon: const Icon(Icons.call_end),
                  label: const Text('Finish call'),
                  style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.red,
                      foregroundColor: Colors.white),
                ),
              ],
            ),
          ],
        );
    }
  }
}