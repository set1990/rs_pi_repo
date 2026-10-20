import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_foreground_task/flutter_foreground_task.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';

const int kListenPort = 8888;
const int kTargetPort = 8889;
const int kCallNotificationId = 1;


@pragma('vm:entry-point')
void startCallback() {
  FlutterForegroundTask.setTaskHandler(UdpTaskHandler());
}


class UdpTaskHandler extends TaskHandler {
  RawDatagramSocket? _socket;
  final FlutterLocalNotificationsPlugin _notifications =
  FlutterLocalNotificationsPlugin();

  bool _ringing = false;
  String _lastSenderIp = '';

  @override
  Future<void> onStart(DateTime timestamp, TaskStarter starter) async {
    await _notifications.initialize(
      const InitializationSettings(
        android: AndroidInitializationSettings('@mipmap/launcher_icon'),
      ),
    );

    try {
      _socket = await RawDatagramSocket.bind(
        InternetAddress.anyIPv4,
        kListenPort,
        reuseAddress: true,
      );
      _socket!.listen(_onSocketEvent);
    } catch (e) {
      // ignore: avoid_print
      print('UDP failed: $e');
    }
  }

  void _onSocketEvent(RawSocketEvent event) {
    if (event != RawSocketEvent.read) return;
    final datagram = _socket?.receive();
    if (datagram == null) return;

    final message = utf8.decode(datagram.data, allowMalformed: true).trim();

    if (message == 'RING_START' && !_ringing) {
      _ringing = true;
      _lastSenderIp = datagram.address.address;
      _showIncomingCall(_lastSenderIp);
      _bringAppToFront();
      FlutterForegroundTask.sendDataToMain({
        'event': 'ring',
        'ip': _lastSenderIp,
      });
    }
  }

  Future<void> _showIncomingCall(String ip) async {
    await _notifications.show(
      kCallNotificationId,
      'Dzwonek do drzwi',
      'Połączenie od $ip',
      NotificationDetails(
        android: AndroidNotificationDetails(
          'calls_v3',
          'Połączenia',
          channelDescription: 'Przychodzące połączenia z domofonu',
          importance: Importance.max,
          priority: Priority.max,
          category: AndroidNotificationCategory.call,
          fullScreenIntent: true,
          ongoing: true,
          autoCancel: false,
          playSound: true,
          sound: const RawResourceAndroidNotificationSound('ring'),
          audioAttributesUsage: AudioAttributesUsage.notificationRingtone,
          additionalFlags: Int32List.fromList(<int>[4]),
          enableVibration: true,
          vibrationPattern: Int64List.fromList(<int>[0, 1000, 1000, 1000]),
        ),
      ),
    );
  }


  Future<void> _bringAppToFront() async {
    try {
      if (await FlutterForegroundTask.canDrawOverlays) {
        FlutterForegroundTask.launchApp();
      }
    } catch (e) {
      // ignore: avoid_print
      print('launchApp failed: $e');
    }
  }

  void _sendUdp(String message) {
    if (_socket == null || _lastSenderIp.isEmpty) return;
    _socket!.send(
      utf8.encode(message),
      InternetAddress(_lastSenderIp),
      kTargetPort,
    );
  }


  @override
  void onReceiveData(Object data) {
    if (data is! Map) return;

    switch (data['cmd']) {
      case 'send':
        final ip = data['ip'];
        if (ip is String && ip.isNotEmpty) _lastSenderIp = ip;
        _sendUdp(data['message'] as String);
        break;
      case 'accepted':
        _notifications.cancel(kCallNotificationId);
        break;
      case 'finish':
        _ringing = false;
        _notifications.cancel(kCallNotificationId);
        break;
      case 'getState':
        FlutterForegroundTask.sendDataToMain({
          'event': 'state',
          'ringing': _ringing,
          'ip': _lastSenderIp,
        });
        break;
    }
  }

  @override
  void onRepeatEvent(DateTime timestamp) {}

  @override
  Future<void> onDestroy(DateTime timestamp) async {
    _notifications.cancel(kCallNotificationId);
    _socket?.close();
    _socket = null;
  }
}