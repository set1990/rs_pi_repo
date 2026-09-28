import socket
import threading
import queue
import re
import tkinter as tk
from tkinter import ttk, messagebox


class LogReceiverApp:
    def __init__(self, root):
        self.root = root
        self.root.title("AsyncLogger - UDP Receiver")
        self.root.geometry("920x550")
        self.root.minsize(750, 400)

        # Stan połączenia i gniazda
        self.sock = None
        self.is_connected = False
        self.log_queue = queue.Queue()

        self._setup_styles()
        self._create_widgets()

        # Pętla odświeżania kolejki logów w interfejsie GUI
        self.root.after(100, self._process_log_queue)

    def _setup_styles(self):
        self.style = ttk.Style()

        # Ustawienie motywu 'clam', który zezwala na zmianę kolorów tła w ttk.Button pod Windows/Linux
        if 'clam' in self.style.theme_names():
            self.style.theme_use('clam')

        # 1. Stan początkowy / Szary (Idle)
        self.style.configure("Idle.TButton", background="#888888", foreground="white", font=("Arial", 10, "bold"))
        self.style.map("Idle.TButton", background=[("active", "#a0a0a0")])

        # 2. Stan połączony / Zielony (Connected)
        self.style.configure("Connected.TButton", background="#28a745", foreground="white", font=("Arial", 10, "bold"))
        self.style.map("Connected.TButton", background=[("active", "#34ce57")])

        # 3. Stan błędu lub rozłączenia / Czerwony (Failed)
        self.style.configure("Failed.TButton", background="#dc3545", foreground="white", font=("Arial", 10, "bold"))
        self.style.map("Failed.TButton", background=[("active", "#e4606d")])

        # 4. Standardowy styl dla przycisku Disconnect
        self.style.configure("Disconnect.TButton", font=("Arial", 10))

    def _create_widgets(self):
        # Górny panel sterowania
        control_frame = ttk.Frame(self.root)
        control_frame.pack(fill=tk.X, padx=10, pady=10)

        ttk.Label(control_frame, text="IP Address:", font=("Arial", 10)).pack(side=tk.LEFT, padx=(0, 5))
        self.ip_entry = ttk.Entry(control_frame, width=15, font=("Arial", 10))
        self.ip_entry.insert(0, "127.0.0.1")
        self.ip_entry.pack(side=tk.LEFT, padx=(0, 15))

        ttk.Label(control_frame, text="Port:", font=("Arial", 10)).pack(side=tk.LEFT, padx=(0, 5))
        self.port_entry = ttk.Entry(control_frame, width=8, font=("Arial", 10))
        self.port_entry.insert(0, "5000")
        self.port_entry.pack(side=tk.LEFT, padx=(0, 15))

        # Przycisk połączenia Connect (ttk.Button) - początkowo Szary
        self.btn_connect = ttk.Button(
            control_frame,
            text="Connect",
            style="Idle.TButton",
            command=self._start_connection
        )
        self.btn_connect.pack(side=tk.LEFT, padx=(0, 5))

        # Przycisk rozłączenia Disconnect (ttk.Button) - początkowo Nieaktywny
        self.btn_disconnect = ttk.Button(
            control_frame,
            text="Disconnect",
            style="Disconnect.TButton",
            state=tk.DISABLED,
            command=lambda: self._disconnect("Disconnected")
        )
        self.btn_disconnect.pack(side=tk.LEFT, padx=(0, 15))

        # Przycisk czyszczenia tabeli
        self.btn_clear = ttk.Button(
            control_frame,
            text="Clear Table",
            command=self.clear_table
        )
        self.btn_clear.pack(side=tk.RIGHT)

        # Tabela z logami
        table_frame = ttk.Frame(self.root)
        table_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))

        columns = ("timestamp", "level", "message")
        self.tree = ttk.Treeview(table_frame, columns=columns, show="headings", selectmode="browse")

        self.tree.heading("timestamp", text="Timestamp")
        self.tree.heading("level", text="Level")
        self.tree.heading("message", text="Message")

        self.tree.column("timestamp", width=170, minwidth=140, stretch=False)
        self.tree.column("level", width=90, minwidth=70, stretch=False)
        self.tree.column("message", width=500, minwidth=200, stretch=True)

        # Kolorowanie poziomów logów
        self.tree.tag_configure("DEBUG", foreground="#17a2b8")
        self.tree.tag_configure("INFO", foreground="#28a745")
        self.tree.tag_configure("ERROR", foreground="#dc3545", font=("Arial", 9, "bold"))

        scrollbar = ttk.Scrollbar(table_frame, orient=tk.VERTICAL, command=self.tree.yview)
        self.tree.configure(yscroll=scrollbar.set)

        self.tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

    def _start_connection(self):
        ip = self.ip_entry.get().strip()
        port_str = self.port_entry.get().strip()

        if not ip or not port_str.isdigit():
            messagebox.showerror("Error", "Please enter a valid IP address and Port number.")
            return

        port = int(port_str)

        # Zmiana opisu na czas próby połączenia
        self.btn_connect.config(text="Connecting...", style="Idle.TButton", state=tk.DISABLED)
        self.btn_disconnect.config(state=tk.DISABLED)

        threading.Thread(target=self._connect_worker, args=(ip, port), daemon=True).start()

    def _connect_worker(self, ip, port):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.settimeout(3.0)

            sock.sendto(b"SUBSCRIBE", (ip, port))

            data, _ = sock.recvfrom(1024)
            response = data.decode("utf-8", errors="ignore")

            if "ACK" in response:
                self.sock = sock
                self.is_connected = True
                self.sock.settimeout(1.0)

                # Połączenie udane: Przycisk Connect robi się ZIELONY, Disconnect zostaje odblokowany
                self.root.after(0, self._update_ui_state, "Connected", "Connected.TButton", tk.DISABLED, tk.NORMAL)

                threading.Thread(target=self._listen_worker, daemon=True).start()
            else:
                raise socket.error("Invalid ACK response from logger")

        except Exception:
            if sock:
                sock.close()
            # Błąd połączenia: Przycisk Connect robi się CZERWONY
            self.root.after(0, self._update_ui_state, "Failed / Retry", "Failed.TButton", tk.NORMAL, tk.DISABLED)

    def _listen_worker(self):
        while self.is_connected and self.sock:
            try:
                data, _ = self.sock.recvfrom(4096)
                if data:
                    msg = data.decode("utf-8", errors="ignore")
                    self.log_queue.put(msg)
            except socket.timeout:
                continue
            except Exception:
                if self.is_connected:
                    self.root.after(0, self._disconnect, "Error")
                break

    def _disconnect(self, button_text="Disconnected"):
        self.is_connected = False
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None

        # Po rozłączeniu: Przycisk Connect staje się CZERWONY ("Disconnected"), Disconnect zostaje zablokowany
        self._update_ui_state(button_text, "Failed.TButton", tk.NORMAL, tk.DISABLED)

    def _update_ui_state(self, connect_text, style_name, connect_state, disconnect_state):
        self.btn_connect.config(text=connect_text, style=style_name, state=connect_state)
        self.btn_disconnect.config(state=disconnect_state)

    def _process_log_queue(self):
        while not self.log_queue.empty():
            raw_msg = self.log_queue.get_nowait()

            for line in raw_msg.strip().splitlines():
                if not line:
                    continue

                match = re.match(r"^\[(.*?)\]\s*\[(.*?)\]\s*(.*)$", line)
                if match:
                    timestamp, level, message = match.groups()
                    tag = level if level in ("DEBUG", "INFO", "ERROR") else ""
                else:
                    timestamp, level, message = "-", "RAW", line
                    tag = ""

                item_id = self.tree.insert("", tk.END, values=(timestamp, level, message), tags=(tag,))
                self.tree.see(item_id)

        self.root.after(100, self._process_log_queue)

    def clear_table(self):
        for item in self.tree.get_children():
            self.tree.delete(item)


if __name__ == "__main__":
    root = tk.Tk()
    app = LogReceiverApp(root)
    root.mainloop()