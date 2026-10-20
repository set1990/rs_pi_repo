import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_foreground_task/flutter_foreground_task.dart';

import 'udp_task_handler.dart';

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

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    FlutterForegroundTask.addTaskDataCallback(_onServiceData);
    WidgetsBinding.instance.addPostFrameCallback((_) => _startService());
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    FlutterForegroundTask.removeTaskDataCallback(_onServiceData);
    super.dispose();
  }

  /// Po powrocie na pierwszy plan zapytaj usługę, czy telefon akurat dzwoni
  /// (np. apka otwarta z powiadomienia pełnoekranowego).
  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed) {
      _sendToService({'cmd': 'getState'});
    }
  }

  // ---------------------------------------------------------------------------
  // Usługa w tle
  // ---------------------------------------------------------------------------

  Future<void> _startService() async {
    if (!Platform.isAndroid) return;

    final permission = await FlutterForegroundTask.checkNotificationPermission();
    if (permission != NotificationPermission.granted) {
      await FlutterForegroundTask.requestNotificationPermission();
    }
    if (!await FlutterForegroundTask.isIgnoringBatteryOptimizations) {
      await FlutterForegroundTask.requestIgnoreBatteryOptimization();
    }

    FlutterForegroundTask.init(
      androidNotificationOptions: AndroidNotificationOptions(
        channelId: 'intercom_service',
        channelName: 'Domofon',
        channelDescription: 'Nasłuchiwanie połączeń z domofonu',
        channelImportance: NotificationChannelImportance.LOW,
        priority: NotificationPriority.LOW,
      ),
      iosNotificationOptions: const IOSNotificationOptions(),
      foregroundTaskOptions: ForegroundTaskOptions(
        eventAction: ForegroundTaskEventAction.nothing(),
        autoRunOnBoot: true,
        allowWakeLock: true,
        allowWifiLock: true,
      ),
    );

    if (await FlutterForegroundTask.isRunningService) {
      await FlutterForegroundTask.restartService();
    } else {
      await FlutterForegroundTask.startService(
        serviceId: 256,
        notificationTitle: 'Domofon aktywny',
        notificationText: 'Oczekiwanie na połączenie',
        callback: startCallback,
      );
    }

    _sendToService({'cmd': 'getState'});
  }

  void _sendToService(Map<String, Object> data) {
    FlutterForegroundTask.sendDataToTask(data);
  }

  void _onServiceData(Object data) {
    if (data is! Map) return;

    final event = data['event'];
    final ringing = event == 'ring' || (event == 'state' && data['ringing'] == true);

    if (ringing && _currentState == AppState.waiting) {
      setState(() {
        _currentState = AppState.ringing;
        _lastSenderIp = (data['ip'] as String?) ?? '';
      });
    }
  }

  void _sendUdpPacket(String message) {
    _sendToService({'cmd': 'send', 'message': message});
  }

  // ---------------------------------------------------------------------------
  // Akcje
  // ---------------------------------------------------------------------------

  void _onAcceptPressed() {
    _sendUdpPacket('CMD_ACCEPT');
    _sendToService({'cmd': 'accepted'});
    setState(() => _currentState = AppState.connected);
  }

  void _onRejectPressed() {
    _sendUdpPacket('CMD_REJECT');
    _sendToService({'cmd': 'finish'});
    setState(() => _currentState = AppState.waiting);
  }

  void _onOpenPressed() {
    _sendUdpPacket('CMD_OPEN');
  }

  void _onEndPressed() {
    _sendUdpPacket('CMD_END');
    _sendToService({'cmd': 'finish'});
    setState(() => _currentState = AppState.waiting);
  }

  // ---------------------------------------------------------------------------
  // UI
  // ---------------------------------------------------------------------------

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

  Widget _buildBodyContent() {
    switch (_currentState) {
      case AppState.waiting:
        return const Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Image(
              image: AssetImage('assets/images/logo.png'),
              width: 300,
              height: 300,
            ),
            SizedBox(height: 20),
            Text(
              'Waiting For The Call...',
              style: TextStyle(fontSize: 18),
              textAlign: TextAlign.center,
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