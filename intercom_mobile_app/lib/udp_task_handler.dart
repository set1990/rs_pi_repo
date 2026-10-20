import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_foreground_task/flutter_foreground_task.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';

const int kListenPort = 8888;
const int kTargetPort = 8889;
const int kCallNotificationId = 1;

/// Punkt wejścia usługi pierwszoplanowej (osobny izolat).
@pragma('vm:entry-point')
void startCallback() {
  FlutterForegroundTask.setTaskHandler(UdpTaskHandler());
}

/// Nasłuchuje UDP w tle, pokazuje powiadomienie o połączeniu
/// i wysyła komendy do domofonu (z tego samego gniazda, portu 8888).
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
          'calls_v2', // nowy id kanału: ustawień istniejącego kanału nie da się zmienić
          'Połączenia',
          channelDescription: 'Przychodzące połączenia z domofonu',
          importance: Importance.max,
          priority: Priority.max,
          category: AndroidNotificationCategory.call,
          fullScreenIntent: true,
          ongoing: true,
          autoCancel: false,
          playSound: true,
          audioAttributesUsage: AudioAttributesUsage.notificationRingtone,
          // FLAG_INSISTENT (4): dźwięk gra w pętli do odebrania/odrzucenia
          additionalFlags: Int32List.fromList(<int>[4]),
          enableVibration: true,
          vibrationPattern: Int64List.fromList(<int>[0, 1000, 1000, 1000]),
        ),
      ),
    );
  }

  void _sendUdp(String message) {
    if (_socket == null || _lastSenderIp.isEmpty) return;
    _socket!.send(
      utf8.encode(message),
      InternetAddress(_lastSenderIp),
      kTargetPort,
    );
  }

  /// Wiadomości z UI.
  @override
  void onReceiveData(Object data) {
    if (data is! Map) return;

    switch (data['cmd']) {
      case 'send':
        _sendUdp(data['message'] as String);
        break;
      case 'accepted': // odebrano: zdejmij powiadomienie, ale nadal "w rozmowie"
        _notifications.cancel(kCallNotificationId);
        break;
      case 'finish': // koniec dzwonienia/rozmowy
        _ringing = false;
        _notifications.cancel(kCallNotificationId);
        break;
      case 'getState': // UI pyta o stan po otwarciu z powiadomienia
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
    _socket?.close();
    _socket = null;
  }
}