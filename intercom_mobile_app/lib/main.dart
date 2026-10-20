import 'dart:io';
import 'dart:convert';
import 'package:flutter/material.dart';

void main() {
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

class _IntercomScreenState extends State<IntercomScreen> {
  AppState _currentState = AppState.waiting;
  RawDatagramSocket? _udpSocket;
  String _lastSenderIp = '';

  final int _listenPort = 8888;
  final int _targetPort = 8889;

  @override
  void initState() {
    super.initState();
    _startUdpServer();
  }

  Future<void> _startUdpServer() async {
    try {
      _udpSocket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, _listenPort);

      _udpSocket!.listen((RawSocketEvent event) {
        if (event == RawSocketEvent.read) {
          Datagram? datagram = _udpSocket!.receive();
          if (datagram != null) {
            String message = utf8.decode(datagram.data).trim();

            if (message == 'RING_START' && _currentState == AppState.waiting) {
              setState(() {
                _currentState = AppState.ringing;
                _lastSenderIp = datagram.address.address;
              });
            }
          }
        }
      });
    } catch (e) {
      print('UDP failed: $e');
    }
  }

  void _sendUdpPacket(String message) {
    if (_udpSocket != null && _lastSenderIp.isNotEmpty) {
      List<int> data = utf8.encode(message);
      _udpSocket!.send(data, InternetAddress(_lastSenderIp), _targetPort);
    }
  }

  void _onAcceptPressed() {
    _sendUdpPacket('CMD_ACCEPT');
    setState(() {
      _currentState = AppState.connected;
    });
  }

  void _onRejectPressed() {
    _sendUdpPacket('CMD_REJECT');
    setState(() {
      _currentState = AppState.waiting;
    });
  }

  void _onOpenPressed() {
    _sendUdpPacket('CMD_OPEN');
  }

  void _onEndPressed() {
    _sendUdpPacket('CMD_END');
    setState(() {
      _currentState = AppState.waiting;
    });
  }

  @override
  void dispose() {
    _udpSocket?.close();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
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
            Text('from: $_lastSenderIp', style: const TextStyle(color: Colors.grey)),
            const SizedBox(height: 40),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: [
                ElevatedButton.icon(
                  onPressed: _onRejectPressed,
                  icon: const Icon(Icons.call_end),
                  label: const Text('Decline'),
                  style: ElevatedButton.styleFrom(backgroundColor: Colors.red, foregroundColor: Colors.white),
                ),
                ElevatedButton.icon(
                  onPressed: _onAcceptPressed,
                  icon: const Icon(Icons.call),
                  label: const Text('Accept'),
                  style: ElevatedButton.styleFrom(backgroundColor: Colors.green, foregroundColor: Colors.white),
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
                  style: ElevatedButton.styleFrom(backgroundColor: Colors.blue, foregroundColor: Colors.white),
                ),
                ElevatedButton.icon(
                  onPressed: _onEndPressed,
                  icon: const Icon(Icons.call_end),
                  label: const Text('Finish call'),
                  style: ElevatedButton.styleFrom(backgroundColor: Colors.red, foregroundColor: Colors.white),
                ),
              ],
            ),
          ],
        );
    }
  }
}