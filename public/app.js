// public/app.js
// ip 192.168.1.7
document.addEventListener('DOMContentLoaded', () => {
    // ... (elemen dan fungsi WebSocket yang sudah ada) ...
    const connectionStatusElement = document.getElementById('connection-status');
    const realtimeAlertElement = document.getElementById('realtime-alert');
    const alertDetailsElement = document.getElementById('alert-details');
    const eventLogElement = document.getElementById('event-log');
    const logPlaceholder = document.querySelector('.log-placeholder');
    const enablePushBtn = document.getElementById('enablePushBtn');
    const pushStatusMsgElement = document.getElementById('push-status-msg');
    const deviceStatusArea = document.getElementById('device-status-area');

    let webSocket;
    let VAPID_PUBLIC_KEY = '';
    let swRegistration = null; // Simpan registrasi Service Worker
    let isSubscribed = false; // Status langganan push

    // --- Logika WebSocket yang sudah ada ---
    function connectWebSocket() {
        const wsUrl = `ws://${window.location.host}`;
        webSocket = new WebSocket(wsUrl);

        webSocket.onopen = () => {
            console.log('Terhubung ke server websocket');
            connectionStatusElement.textContent = 'Terhubung ke sistem notifikasi';
            connectionStatusElement.style.color = '#28a745';
        };

        webSocket.onmessage = (event) => {
            const message = JSON.parse(event.data);
            console.log('Menerima pesan: ', message);

            if (message.type === 'fall_detected') {
                handleFallDetected(message.data);
                addEventToLog(message.data);
            } else if (message.type === 'history') {
                populateEventLog(message.data);
            // BAGIAN BARU UNTUK MENANGANI STATUS
            } else if (message.type === 'state_update') {
                updateDeviceStatus(message.data);
            } else if (message.type === 'initial_states') {
                // Tampilkan semua status perangkat saat pertama kali terhubung
                for (const deviceId in message.data) {
                    updateDeviceStatus({ deviceId: deviceId, state: message.data[deviceId].state });
                }
            }
        };

        webSocket.onclose = () => {
            console.log('Koneksi WebSocket terputus. Mencoba menghubungkan kembali...');
            connectionStatusElement.textContent = 'Koneksi terputus. Mencoba menghubungkan ulang...';
            connectionStatusElement.style.color = '#dc3545'; //Merah
            setTimeout(connectWebSocket, 3000); //Coba menghubungkan lagi setelah 3 detik
        };

        webSocket.onerror = (error) => {
            console.error('WebSocket error: ', error);
            connectionStatusElement.textContent = 'Gagal terhubung ke server.';
        };
    }

    function updateDeviceStatus(data) {
        const { deviceId, state } = data;
        // Cari kartu perangkat yang sudah ada berdasarkan ID unik
        let deviceCard = document.getElementById(`device-${deviceId}`);

        if (!deviceCard) {
            // Jika belum ada, buat elemen div baru untuk perangkat ini
            deviceCard = document.createElement('div');
            deviceCard.id = `device-${deviceId}`; // Beri ID agar bisa ditemukan lagi
            deviceStatusArea.appendChild(deviceCard);
        }

        // Update isi HTML dan kelas CSS untuk mengubah warna
        deviceCard.innerHTML = `
            <strong>ID Perangkat: ${deviceId}</strong>
            <span>Status Saat Ini: <strong>${state}</strong></span>
        `;
        // Hapus kelas warna lama dan tambahkan yang baru sesuai status
        deviceCard.className = 'device-card'; // Reset kelas ke default
        deviceCard.classList.add(`state-${state}`);
    }

    function handleFallDetected(data) {
        alertDetailsElement.innerHTML = `
            <strong>Perangkat:</strong> ${data.deviceId}<br>
            <strong>Lokasi:</strong> ${data.location}<br>
            <strong>Waktu:</strong> ${new Date(data.timestamp).toLocaleString()}
        `;
        realtimeAlertElement.classList.remove('alert-hidden');
        // Mainkan suara notifikasi (opsional)
        // const audio = new Audio('/sounds/alert.mp3');
        // audio.play();
    }

    function addEventToLog(data, isHistory = false){
        if (logPlaceholder){
            logPlaceholder.remove();
        }

        const li = document.createElement('li');
        li.classList.add('fall-event');
        li.innerHTML = `
            <span class="details"><strong>Jatuh Terdeteksi!</strong> - Perangkat: ${data.deviceId}</span>
            <span class="timestamp">${new Date(data.timestamp).toLocaleString()}</span>
        `;

        if (isHistory) {
            eventLogElement.appendChild(li);
        } else {
            eventLogElement.insertBefore(li, eventLogElement.firstChild);
        }
    }

    function populateEventLog(historyData){
        eventLogElement.innerHTML = '';
        if (historyData && historyData.length > 0){
            [...historyData].reverse().forEach(event => addEventToLog(event, true));
        } else {
            eventLogElement.innerHTML = '<li class="log-placeholder">Belum ada kejadian tercatat.</li>';
        }
    }

    // connectWebSocket(); // Panggil fungsi WebSocket
    // --- Akhir Logika WebSocket ---


    // Fungsi untuk mengambil VAPID public key dari server
    async function fetchVapidPublicKey() {
        try {
            const response = await fetch('/api/vapid-public-key');
            if (!response.ok) throw new Error('Gagal mengambil VAPID public key dari server.');
            VAPID_PUBLIC_KEY = await response.text();
            console.log('VAPID Public Key diterima:', VAPID_PUBLIC_KEY);
            if (VAPID_PUBLIC_KEY) {
                initializePushNotifications(); // Lanjutkan inisialisasi setelah key diterima
            } else {
                 if (pushStatusMsgElement) pushStatusMsgElement.textContent = 'Status: Gagal memuat konfigurasi push server.';
            }
        } catch (error) {
            console.error('Error fetching VAPID public key:', error);
            if (pushStatusMsgElement) pushStatusMsgElement.textContent = `Status: Error - ${error.message}`;
        }
    }

    // Konversi string base64 url-safe ke Uint8Array
    function urlBase64ToUint8Array(base64String) {
        const padding = '='.repeat((4 - base64String.length % 4) % 4);
        const base64 = (base64String + padding).replace(/-/g, '+').replace(/_/g, '/');
        const rawData = window.atob(base64);
        const outputArray = new Uint8Array(rawData.length);
        for (let i = 0; i < rawData.length; ++i) {
            outputArray[i] = rawData.charCodeAt(i);
        }
        return outputArray;
    }

    // Inisialisasi dan cek status langganan push
    async function initializePushNotifications() {
        if (!('serviceWorker' in navigator) || !('PushManager' in window)) {
            console.warn('Push messaging tidak didukung oleh browser ini.');
            if (pushStatusMsgElement) pushStatusMsgElement.textContent = 'Status: Notifikasi push tidak didukung browser ini.';
            if (enablePushBtn) enablePushBtn.disabled = true;
            return;
        }

        try {
            swRegistration = await navigator.serviceWorker.register('/service-worker.js');
            console.log('Service Worker berhasil diregistrasi:', swRegistration);

            // Cek status langganan yang sudah ada
            const subscription = await swRegistration.pushManager.getSubscription();
            isSubscribed = !(subscription === null);
            updatePushBtnUI();

            if (isSubscribed) {
                console.log('Pengguna sudah berlangganan notifikasi push.');
            } else {
                console.log('Pengguna belum berlangganan.');
            }
        } catch (error) {
            console.error('Gagal registrasi Service Worker:', error);
            if (pushStatusMsgElement) pushStatusMsgElement.textContent = 'Status: Gagal setup notifikasi push.';
        }
    }

    // Update UI tombol push
    function updatePushBtnUI() {
        if (!enablePushBtn || !pushStatusMsgElement) return;

        if (Notification.permission === 'denied') {
            pushStatusMsgElement.textContent = 'Status: Izin notifikasi diblokir.';
            enablePushBtn.disabled = true;
            return;
        }

        if (isSubscribed) {
            enablePushBtn.textContent = 'Nonaktifkan Notifikasi Push';
            pushStatusMsgElement.textContent = 'Status: Notifikasi push aktif.';
        } else {
            enablePushBtn.textContent = 'Aktifkan Notifikasi Push';
            pushStatusMsgElement.textContent = 'Status: Notifikasi push belum aktif.';
        }
        enablePushBtn.disabled = false;
    }

    // Fungsi untuk berlangganan push
    async function subscribeUserToPush() {
        if (!swRegistration || !VAPID_PUBLIC_KEY) {
            console.error("Service worker atau VAPID key belum siap.");
            if(pushStatusMsgElement) pushStatusMsgElement.textContent = 'Status: Konfigurasi belum siap.';
            return;
        }
        try {
            const applicationServerKey = urlBase64ToUint8Array(VAPID_PUBLIC_KEY);
            const subscription = await swRegistration.pushManager.subscribe({
                userVisibleOnly: true,
                applicationServerKey: applicationServerKey
            });
            console.log('Berhasil berlangganan push:', JSON.stringify(subscription));

            // Kirim langganan ke server
            await sendSubscriptionToServer(subscription);
            isSubscribed = true;
            updatePushBtnUI();

        } catch (error) {
            if (Notification.permission === 'denied') {
                console.warn('Izin notifikasi ditolak oleh pengguna.');
            } else {
                console.error('Gagal berlangganan push:', error);
            }
            isSubscribed = false;
            updatePushBtnUI();
        }
    }

    // Fungsi untuk berhenti berlangganan
    async function unsubscribeUserFromPush() {
        if (!swRegistration) return;
        try {
            const subscription = await swRegistration.pushManager.getSubscription();
            if (subscription) {
                await subscription.unsubscribe();
                console.log('Berhasil berhenti berlangganan.');
                // Kirim info unsubscribe ke server (opsional, untuk membersihkan database)
                await removeSubscriptionFromServer(subscription);
                isSubscribed = false;
            }
        } catch (error) {
            console.error('Gagal berhenti berlangganan:', error);
        }
        updatePushBtnUI();
    }


    // Kirim langganan ke server
    async function sendSubscriptionToServer(subscription) {
        try {
            const response = await fetch('/api/subscribe', {
                method: 'POST',
                body: JSON.stringify(subscription),
                headers: { 'Content-Type': 'application/json' }
            });
            if (!response.ok) throw new Error('Server gagal menyimpan langganan.');
            console.log('Langganan berhasil dikirim ke server.');
        } catch (error) {
            console.error('Gagal mengirim langganan ke server:', error);
            // Jika gagal, mungkin batalkan langganan di sisi klien
            if (isSubscribed) {
                await unsubscribeUserFromPush(); // Coba unsubscribe lagi
            }
            throw error; // Lemparkan error agar bisa ditangani lebih lanjut
        }
    }

    // Hapus langganan dari server (opsional)
    async function removeSubscriptionFromServer(subscription) {
        try {
            const response = await fetch('/api/unsubscribe', {
                method: 'POST',
                body: JSON.stringify({ endpoint: subscription.endpoint }),
                headers: { 'Content-Type': 'application/json' }
            });
            if (!response.ok) throw new Error('Server gagal menghapus langganan.');
            console.log('Langganan berhasil dihapus dari server.');
        } catch (error) {
            console.error('Gagal menghapus langganan dari server:', error);
        }
    }


    if (enablePushBtn) {
        enablePushBtn.addEventListener('click', () => {
            enablePushBtn.disabled = true; // Cegah double click
            if (isSubscribed) {
                unsubscribeUserFromPush();
            } else {
                // Meminta izin dulu, lalu subscribe jika diizinkan
                Notification.requestPermission().then(permission => {
                    if (permission === 'granted') {
                        subscribeUserToPush();
                    } else {
                        console.warn('Izin notifikasi ditolak.');
                        updatePushBtnUI(); // Update UI berdasarkan izin
                    }
                }).catch(err => {
                    console.error("Error meminta izin notifikasi:", err);
                    updatePushBtnUI();
                });
            }
        });
    }

    // --- Panggil fungsi-fungsi yang sudah ada & yang baru ---
    // (Pastikan fungsi WebSocket seperti connectWebSocket() dipanggil di sini)
    connectWebSocket(); // Panggil fungsi koneksi WebSocket yang sudah ada
    fetchVapidPublicKey(); // Panggil untuk memulai setup push notif
});